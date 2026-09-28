#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Driver Engineer");
MODULE_DESCRIPTION("Module Lifecycle & Parameters Demo");

static int debug_level = 1;
static char *device_name = "default_device";

module_param(debug_level, int, 0644);
MODULE_PARM_DESC(debug_level, "Debug verbosity level (int)");

module_param(device_name, charp, 0444);
MODULE_PARM_DESC(device_name, "Target device name (string)");

static int __init mod_lifecycle_init(void)
{
    pr_info("mod_lifecycle: Loaded with device_name='%s', debug_level=%d\n",
            device_name, debug_level);
    return 0; // Return 0 indicates successful initialization
}

static void __exit mod_lifecycle_exit(void)
{
    pr_info("mod_lifecycle: Unloading module. Goodbye!\n");
}

module_init(mod_lifecycle_init);
module_exit(mod_lifecycle_exit);
