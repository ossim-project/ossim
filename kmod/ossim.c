#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ossim Project");
MODULE_DESCRIPTION("Ossim Kernel Module");
MODULE_VERSION("0.1");

static int __init ossim_init(void)
{
    printk(KERN_INFO "Hello, Ossim module loaded!\n");
    return 0;
}

static void __exit ossim_exit(void)
{
    printk(KERN_INFO "Goodbye, Ossim module unloaded!\n");
}

module_init(ossim_init);
module_exit(ossim_exit);