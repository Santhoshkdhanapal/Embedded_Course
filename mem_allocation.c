#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>       /* For kmalloc / kfree */
#include <asm/io.h>           /* For virt_to_phys */

MODULE_LICENSE("GPL");

static void *k_ptr;
static void *v_ptr;

static int __init mem_demo_init(void)
{
    // Allocate 4KB via kmalloc (Physically Contiguous)
    k_ptr = kmalloc(4096, GFP_KERNEL);
    if (!k_ptr) {
        pr_err("mem_allocation: kmalloc failed\n");
        return -ENOMEM;
    }
    pr_info("mem_allocation: kmalloc allocated at Virtual Addr: %px, Physical: %llx\n",
            k_ptr, (unsigned long long)virt_to_phys(k_ptr));

    // Allocate 1MB via vmalloc (Virtually Contiguous, Physically Fragmented)
    v_ptr = vmalloc(1024 * 1024);
    if (!v_ptr) {
        pr_err("mem_allocation: vmalloc failed\n");
        kfree(k_ptr);
        return -ENOMEM;
    }
    pr_info("mem_allocation: vmalloc allocated 1MB at Virtual Addr: %px\n", v_ptr);

    return 0;
}

static void __exit mem_demo_exit(void)
{
    kfree(k_ptr);
    vfree(v_ptr);
    pr_info("mem_allocation: Memory released successfully\n");
}

module_init(mem_demo_init);
module_exit(mem_demo_exit);


