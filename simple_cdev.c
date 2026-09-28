#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define DEVICE_NAME "sample_cdev"
#define BUFFER_SIZE 512

MODULE_LICENSE("GPL");

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *cdev_class = NULL;
static char kernel_buffer[BUFFER_SIZE];
static size_t data_size = 0;

static int dev_open(struct inode *inodep, struct file *filep)
{
    pr_info("simple_cdev: Device opened\n");
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    pr_info("simple_cdev: Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    size_t bytes_to_copy;

    if (*offset >= data_size)
        return 0; // EOF

    bytes_to_copy = min(len, (size_t)(data_size - *offset));

    if (copy_to_user(buffer, kernel_buffer + *offset, bytes_to_copy))
        return -EFAULT;

    *offset += bytes_to_copy;
    pr_info("simple_cdev: Read %zu bytes\n", bytes_to_copy);
    return bytes_to_copy;
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    size_t bytes_to_copy = min(len, (size_t)(BUFFER_SIZE - 1));

    if (copy_from_user(kernel_buffer, buffer, bytes_to_copy))
        return -EFAULT;

    kernel_buffer[bytes_to_copy] = '\0';
    data_size = bytes_to_copy;
    *offset += bytes_to_copy;

    pr_info("simple_cdev: Received %zu bytes from user\n", bytes_to_copy);
    return bytes_to_copy;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

static int __init simple_cdev_init(void)
{
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0)
        return -1;

    cdev_init(&my_cdev, &fops);
    if (cdev_add(&my_cdev, dev_num, 1) < 0)
        goto unregister_region;

    cdev_class = class_create(DEVICE_NAME);
    if (IS_ERR(cdev_class))
        goto del_cdev;

    if (IS_ERR(device_create(cdev_class, NULL, dev_num, NULL, DEVICE_NAME)))
        goto destroy_class;

    pr_info("simple_cdev: Device node created under /dev/%s\n", DEVICE_NAME);
    return 0;

destroy_class:
    class_destroy(cdev_class);
del_cdev:
    cdev_del(&my_cdev);
unregister_region:
    unregister_chrdev_region(dev_num, 1);
    return -1;
}

static void __exit simple_cdev_exit(void)
{
    device_destroy(cdev_class, dev_num);
    class_destroy(cdev_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("simple_cdev: Unregistered successfully\n");
}

module_init(simple_cdev_init);
module_exit(simple_cdev_exit);

