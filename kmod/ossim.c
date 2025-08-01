#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/errno.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ossim Project");
MODULE_DESCRIPTION("Ossim Kernel Module");
MODULE_VERSION("0.1");

static long ossim_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int data;
	long ret;
	ret = copy_from_user(&data, (int __user *)arg, sizeof(data));
	pr_info("[ossim] ioctl: cmd=%u arg=%d\n", cmd, data);
	return 0;
}

static const struct file_operations ossim_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ossim_ioctl,
};

static struct miscdevice ossim_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ossim",
	.fops = &ossim_fops,
	.mode = 0666,
};

static void do_cleanup(void)
{
	misc_deregister(&ossim_miscdev);
}

static int __init ossim_init(void)
{
	int ret;
	pr_info("[ossim] Initializing Ossim\n");

	ret = misc_register(&ossim_miscdev);
	if (ret < 0) {
		pr_err("[ossim] Failed to register misc device: %d\n", ret);
		goto err;
	}

	printk(KERN_INFO "[ossim] Module loaded!\n");
	return 0;

err:
	do_cleanup();
	return ret;
}

static void __exit ossim_exit(void)
{
	do_cleanup();
	pr_info("[ossim] Module unloaded!\n");
}

module_init(ossim_init);
module_exit(ossim_exit);