#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#define IRQ_NUM 1 // Standard PC Keyboard interrupt

MODULE_LICENSE("GPL");

static struct workqueue_struct *my_wq;
static struct work_struct my_work;

static void workqueue_handler(struct work_struct *work)
{
    // Process context: sleeping and blocking operations are safe here
    pr_info("interrupt_engine: Workqueue executing. Context is can-sleep.\n");
    msleep(10); 
    pr_info("interrupt_engine: Workqueue completed.\n");
}

// Top-half handler (Atomic / Hard IRQ context)
static irqreturn_t hard_irq_handler(int irq, void *dev_id)
{
    pr_info("interrupt_engine: Top-half ISR executed for IRQ %d\n", irq);
    
    // Defer work to custom workqueue
    if (my_wq)
        queue_work(my_wq, &my_work);

    // Awaken dedicated IRQ thread for bottom-half execution
    return IRQ_WAKE_THREAD;
}

// Bottom-half threaded IRQ handler (Process context)
static irqreturn_t threaded_irq_handler(int irq, void *dev_id)
{
    pr_info("interrupt_engine: Threaded IRQ bottom-half running in PID %d (%s)\n",
            current->pid, current->comm);
    return IRQ_HANDLED;
}

static int __init irq_demo_init(void)
{
    int ret;

    my_wq = create_singlethread_workqueue("custom_irq_wq");
    INIT_WORK(&my_work, workqueue_handler);

    // IRQF_SHARED is mandatory when sharing IRQ lines (like IRQ 1 on standard PCs)
    ret = request_threaded_irq(IRQ_NUM, hard_irq_handler, threaded_irq_handler,
                               IRQF_SHARED, "interrupt_engine_dev", (void *)(irq_demo_init));
    if (ret) {
        pr_err("interrupt_engine: Failed to request IRQ %d (Error %d)\n", IRQ_NUM, ret);
        destroy_workqueue(my_wq);
        return ret;
    }

    pr_info("interrupt_engine: Registered threaded ISR on IRQ %d\n", IRQ_NUM);
    return 0;
}

static void __exit irq_demo_exit(void)
{
    free_irq(IRQ_NUM, (void *)(irq_demo_init));
    cancel_work_sync(&my_work);
    destroy_workqueue(my_wq);
    pr_info("interrupt_engine: IRQ handler freed\n");
}

module_init(irq_demo_init);
module_exit(irq_demo_exit);
eof
