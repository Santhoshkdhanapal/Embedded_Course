#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Engineer");
MODULE_DESCRIPTION("Top-half, Threaded IRQ, and Workqueue Engine on IRQ 1");
MODULE_VERSION("1.0");

#define KEYBOARD_IRQ 1

static void *dev_id = &dev_id; /* Unique tok#en for shared IRQ handling */
static struct workqueue_struct *engine_wq;
static struct work_struct engine_work;

/* -------------------------------------------------------------
 * 3. Workqueue Handler (Process Context / Can Sleep)
 * ------------------------------------------------------------- */
static void engine_workqueue_handler(struct work_struct *work)
{
    pr_info("interrupt_engine: Workqueue executing. Context is can-sleep.\n");
    
    /* Simulate deferred non-atomic operation (10ms delay matches dmesg timestamps) */
    msleep(10);
    
    pr_info("interrupt_engine: Workqueue completed.\n");
}

/* -------------------------------------------------------------
 * 2. Threaded IRQ Bottom-Half Handler (kthread context: irq/1-*)
 * ------------------------------------------------------------- */
static irqreturn_t engine_threaded_irq_handler(int irq, void *dev_id)
{
    pr_info("interrupt_engine: Threaded IRQ bottom-half running in PID %d (%s)\n",
            current->pid, current->comm);

    /* Queue heavy background work to the dedicated workqueue */
    if (engine_wq) {
        queue_work(engine_wq, &engine_work);
    }

    return IRQ_HANDLED;
}

/* -------------------------------------------------------------
 * 1. Primary Top-Half ISR (Hard-IRQ context, atomic, must not sleep)
 * ------------------------------------------------------------- */
static irqreturn_t engine_top_half_isr(int irq, void *dev_id)
{
    pr_info("interrupt_engine: Top-half ISR executed for IRQ %d\n", irq);

    /* Wake up the threaded IRQ handler (irq/1-*) */
    return IRQ_WAKE_THREAD;
}

/* -------------------------------------------------------------
 * Module Init & Exit
 * ------------------------------------------------------------- */
static int __init interrupt_engine_init(void)
{
    int ret;

    /* Initialize work item */
    INIT_WORK(&engine_work, engine_workqueue_handler);

    /* Create dedicated single-thread workqueue */
    engine_wq = create_singlethread_workqueue("engine_wq");
    if (!engine_wq) {
        pr_err("interrupt_engine: Failed to create workqueue\n");
        return -ENOMEM;
    }

    /*
     * Register shared threaded IRQ on IRQ 1 (Standard x86 PS/2 i8042 Keyboard).
     * IRQF_SHARED is mandatory since the i8042/atkbd driver already owns IRQ 1.
     */
    ret = request_threaded_irq(KEYBOARD_IRQ,
                               engine_top_half_isr,
                               engine_threaded_irq_handler,
                               IRQF_SHARED,
                               "interrupt_engine",
                               dev_id);
    if (ret) {
        pr_err("interrupt_engine: Failed to request IRQ %d (err: %d)\n", KEYBOARD_IRQ, ret);
        destroy_workqueue(engine_wq);
        return ret;
    }

    pr_info("interrupt_engine: Loaded and hooked to IRQ %d\n", KEYBOARD_IRQ);
    return 0;
}

static void __exit interrupt_engine_exit(void)
{
    /* 1. Unregister IRQ line so no new ISRs trigger */
    free_irq(KEYBOARD_IRQ, dev_id);

    /* 2. Flush and destroy the workqueue to ensure all pending work completes */
    if (engine_wq) {
        flush_workqueue(engine_wq);
        destroy_workqueue(engine_wq);
    }

    pr_info("interrupt_engine: Unloaded successfully\n");
}

module_init(interrupt_engine_init);
module_exit(interrupt_engine_exit);

