#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/jiffies.h>
#include <linux/seq_file.h>

#define PROC_NAME "student_info"

static unsigned long load_time;
static int read_count = 0;
static int proc_show(struct seq_file *m, void *v) {
    read_count++;
    
    seq_printf(m, "Name: Kaiser Yury\n");
    seq_printf(m, "Module loaded at: %lu jiffies\n", load_time);
    seq_printf(m, "Current jiffies: %lu\n", jiffies);
    seq_printf(m, "Uptime: %lu seconds\n", jiffies / HZ);
    seq_printf(m, "Read count: %d\n", read_count);
    
    return 0;
}

static int proc_open(struct inode *inode, struct file *file) {
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_ops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init proc_init(void) {
    load_time = jiffies;
    
    // Создаём файл в /proc
    proc_create(PROC_NAME, 0444, NULL, &proc_ops);
    printk(KERN_INFO "=== PROC MODULE LOADED ===\n");
    printk(KERN_INFO "Proc file /proc/%s created\n", PROC_NAME);
    printk(KERN_INFO "Module loaded at jiffies: %lu\n", load_time);
    
    return 0;
}

static void __exit proc_exit(void) {
    // Удаляем файл из /proc
    remove_proc_entry(PROC_NAME, NULL);
    printk(KERN_INFO "=== PROC MODULE UNLOADED ===\n");
    printk(KERN_INFO "Proc file /proc/%s removed\n", PROC_NAME);
    printk(KERN_INFO "Total reads: %d\n", read_count);
}

module_init(proc_init);
module_exit(proc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kaiser Yury");
MODULE_DESCRIPTION("/proc file system module");
