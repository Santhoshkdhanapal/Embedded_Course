#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");

static DEFINE_SPINLOCK(fast_lock);
static DEFINE_MUTEX(slow_lock);
static atomic_t counter = ATOMIC_INIT(0);

static struct task_struct *worker_thread;

static int sync_worker(void *data)
{
    unsigned long flags;

    // 1. Atomic Primitive test
    atomic_inc(&counter);
    pr_info("sync_primitives: Atomic counter incremented to: %d\n", atomic_read(&counter));

    // 2. Spinlock Critical Section
    spin_lock_irqsave(&fast_lock, flags);
    // CRITICAL SECTION: Non-blocking, ultra-fast calculations
    pr_info("sync_primitives: Spinlock acquired in thread context\n");
    spin_unlock_irqrestore(&fast_lock, flags);

    // 3. Mutex Critical Section
    mutex_lock(&slow_lock);
    // CRITICAL SECTION: Safe to sleep or perform file/memory work
    pr_info("sync_primitives: Mutex acquired, entering controlled sleep\n");
    msleep(50);
    mutex_unlock(&slow_lock);

    return 0;
}

static int __init sync_init(void)
{
    pr_info("sync_primitives: Initializing\n");
    worker_thread = kthread_run(sync_worker, NULL, "sync_test_worker");
    return 0;
}

static void __exit sync_exit(void)
{
    pr_info("sync_primitives: Exiting\n");
}

module_init(sync_init);
module_exit(sync_exit);
