#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/cred.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/ktime.h>
#include "kshield_common.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Systems Security Engineering");
MODULE_DESCRIPTION("K-Shield: Process Execution Telemetry Driver");
MODULE_VERSION("1.1");

static int kshield_major;
static struct class *kshield_class = NULL;
static struct device *kshield_dev = NULL;
static struct cdev kshield_cdev;

static struct kshield_telemetry ring_buffer[KSHIELD_RING_CAPACITY];
static uint32_t ring_head = 0;
static uint32_t ring_tail = 0;
static uint64_t dropped_counter = 0;

static spinlock_t buffer_lock;
static wait_queue_head_t read_waitqueue;

static inline bool is_buffer_empty(void) {
    return ring_head == ring_tail;
}

static inline bool is_buffer_full(void) {
    return ((ring_head + 1) % KSHIELD_RING_CAPACITY) == ring_tail;
}

static struct kprobe execve_kp;

static int handler_execve_pre(struct kprobe *p, struct pt_regs *regs) {
    struct kshield_telemetry event;
    const char __user *filename_user_ptr;
    unsigned long irq_flags;
    long copy_res;

    memset(&event, 0, sizeof(struct kshield_telemetry));
    event.event_type = KSHIELD_EVENT_EXECVE;
    event.pid = current->pid;
    event.ppid = current->real_parent ? current->real_parent->pid : 0;
    event.uid = from_kuid(&init_user_ns, current_cred()->uid);
    event.timestamp_ns = ktime_get_real_ns();
    strncpy(event.comm, current->comm, KSHIELD_COMM_MAX - 1);

#if defined(__x86_64__)
    filename_user_ptr = (const char __user *)regs->di;
#elif defined(__aarch64__)
    filename_user_ptr = (const char __user *)regs->regs[0];
#else
    filename_user_ptr = NULL;
#endif

    if (filename_user_ptr) {
        copy_res = strncpy_from_user(event.target_path, filename_user_ptr, KSHIELD_PATH_MAX - 1);
        if (copy_res < 0) {
            strncpy(event.target_path, "<fault_user_mem>", KSHIELD_PATH_MAX - 1);
        }
    } else {
        strncpy(event.target_path, "<null_arg>", KSHIELD_PATH_MAX - 1);
    }

    spin_lock_irqsave(&buffer_lock, irq_flags);
    if (!is_buffer_full()) {
        ring_buffer[ring_head] = event;
        ring_head = (ring_head + 1) % KSHIELD_RING_CAPACITY;
        wake_up_interruptible(&read_waitqueue);
    } else {
        dropped_counter++;
    }
    spin_unlock_irqrestore(&buffer_lock, irq_flags);

    return 0;
}

static int kshield_open(struct inode *inode, struct file *file) {
    return 0;
}

static int kshield_release(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t kshield_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    struct kshield_telemetry item;
    unsigned long irq_flags;
    int ret;

    if (count < sizeof(struct kshield_telemetry))
        return -EINVAL;

    if (file->f_flags & O_NONBLOCK) {
        if (is_buffer_empty())
            return -EAGAIN;
    } else {
        ret = wait_event_interruptible(read_waitqueue, !is_buffer_empty());
        if (ret)
            return ret;
    }

    spin_lock_irqsave(&buffer_lock, irq_flags);
    if (is_buffer_empty()) {
        spin_unlock_irqrestore(&buffer_lock, irq_flags);
        return 0;
    }

    item = ring_buffer[ring_tail];
    ring_tail = (ring_tail + 1) % KSHIELD_RING_CAPACITY;
    spin_unlock_irqrestore(&buffer_lock, irq_flags);

    if (copy_to_user(user_buf, &item, sizeof(struct kshield_telemetry)))
        return -EFAULT;

    return sizeof(struct kshield_telemetry);
}

static __poll_t kshield_poll(struct file *file, poll_table *wait) {
    __poll_t mask = 0;
    poll_wait(file, &read_waitqueue, wait);

    spin_lock(&buffer_lock);
    if (!is_buffer_empty()) {
        mask |= (EPOLLIN | EPOLLRDNORM);
    }
    spin_unlock(&buffer_lock);

    return mask;
}

static long kshield_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    unsigned long irq_flags;

    switch (cmd) {
    case KSHIELD_IOCTL_GET_DROPPED_COUNT:
        if (copy_to_user((uint64_t __user *)arg, &dropped_counter, sizeof(uint64_t)))
            return -EFAULT;
        break;

    case KSHIELD_IOCTL_PURGE_BUFFER:
        spin_lock_irqsave(&buffer_lock, irq_flags);
        ring_head = 0;
        ring_tail = 0;
        spin_unlock_irqrestore(&buffer_lock, irq_flags);
        break;

    default:
        return -ENOTTY;
    }
    return 0;
}

static const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = kshield_open,
    .release        = kshield_release,
    .read           = kshield_read,
    .poll           = kshield_poll,
    .unlocked_ioctl = kshield_ioctl,
};

static int __init kshield_driver_init(void) {
    dev_t dev_id;
    int res;

    spin_lock_init(&buffer_lock);
    init_waitqueue_head(&read_waitqueue);

    res = alloc_chrdev_region(&dev_id, 0, 1, KSHIELD_DEVICE_NAME);
    if (res < 0) return res;
    kshield_major = MAJOR(dev_id);

    cdev_init(&kshield_cdev, &fops);
    kshield_cdev.owner = THIS_MODULE;
    res = cdev_add(&kshield_cdev, dev_id, 1);
    if (res < 0) goto err_unregister_chrdev;

    kshield_class = class_create(KSHIELD_CLASS_NAME);
    if (IS_ERR(kshield_class)) {
        res = PTR_ERR(kshield_class);
        goto err_del_cdev;
    }

    kshield_dev = device_create(kshield_class, NULL, dev_id, NULL, KSHIELD_DEVICE_NAME);
    if (IS_ERR(kshield_dev)) {
        res = PTR_ERR(kshield_dev);
        goto err_destroy_class;
    }

    execve_kp.symbol_name = "__x64_sys_execve";
    execve_kp.pre_handler = handler_execve_pre;
    res = register_kprobe(&execve_kp);
    if (res < 0) goto err_destroy_device;

    pr_info("[K-Shield] Engine online: /dev/%s ready.\n", KSHIELD_DEVICE_NAME);
    return 0;

err_destroy_device:
    device_destroy(kshield_class, dev_id);
err_destroy_class:
    class_destroy(kshield_class);
err_del_cdev:
    cdev_del(&kshield_cdev);
err_unregister_chrdev:
    unregister_chrdev_region(MKDEV(kshield_major, 0), 1);
    return res;
}

static void __exit kshield_driver_exit(void) {
    dev_t dev_id = MKDEV(kshield_major, 0);

    unregister_kprobe(&execve_kp);
    device_destroy(kshield_class, dev_id);
    class_destroy(kshield_class);
    cdev_del(&kshield_cdev);
    unregister_chrdev_region(dev_id, 1);

    pr_info("[K-Shield] Engine unloaded successfully.\n");
}

module_init(kshield_driver_init);
module_exit(kshield_driver_exit);
