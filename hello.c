#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Sanity Check LKM");

static int __init hello_init(void) {
    pr_info("Lab: Hello from Kernel Space!\n");
    return 0;
}

static void __exit hello_exit(void) {
    pr_info("Lab: Goodbye from Kernel Space!\n");
}

module_init(hello_init);
module_exit(hello_exit);
