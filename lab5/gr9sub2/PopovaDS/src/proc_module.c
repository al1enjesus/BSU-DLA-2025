#include <linux/module.h>


static ssize_t proc_read(struct file *file, char __user *ubuf,
size_t count, loff_t *ppos)
{
char buf[MAX_SIZE];
int len;


if (*ppos > 0)
return 0; 


read_count++;


len = snprintf(buf, sizeof(buf),
"Name: Dasha\n"
"Group: <9>, Subgroup: <2>\n"
"Module loaded at: %lu jiffies\n"
"Read count: %d\n",
load_time, read_count);


if (len < 0)
return -EFAULT;


if (len > count)
len = count; // respect user buffer


if (copy_to_user(ubuf, buf, len))
return -EFAULT;


*ppos = len;
return len;
}


static const struct proc_ops proc_file_ops = {
.proc_read = proc_read,
};


static int __init proc_module_init(void)
{
printk(KERN_INFO "proc_module: Initializing\n");


load_time = jiffies;


proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_file_ops);
if (!proc_file) {
printk(KERN_ERR "proc_module: Failed to create /proc/%s\n", PROC_NAME);
return -ENOMEM;
}


printk(KERN_INFO "proc_module: Created /proc/%s\n", PROC_NAME);
return 0;
}


static void __exit proc_module_exit(void)
{
if (proc_file) {
proc_remove(proc_file);
proc_file = NULL;
printk(KERN_INFO "proc_module: Removed /proc/%s\n", PROC_NAME);
}
}


module_init(proc_module_init);
module_exit(proc_module_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dasha <student@example.com>");
MODULE_DESCRIPTION("Proc filesystem example");
MODULE_VERSION("1.0");
