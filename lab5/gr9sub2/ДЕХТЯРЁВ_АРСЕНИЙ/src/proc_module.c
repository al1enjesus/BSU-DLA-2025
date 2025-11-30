#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Дехтярёв Арсений");
MODULE_DESCRIPTION("Proc file module for lab");
MODULE_VERSION("0.1");

#define PROC_FILENAME "dehtyarev_info"

static int proc_show(struct seq_file *m, void *v)
{
    seq_printf(m,
        "ФИО: Дехтярёв Арсений\n"
        "Группа: 9\n"
        "Подгруппа: 2\n");
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_file_ops = {
    .proc_open    = proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init proc_module_init(void)
{
    // Создаем файл с правами доступа 0444 (только чтение для всех)
    if (!proc_create(PROC_FILENAME, 0444, NULL, &proc_file_ops)) {
        printk(KERN_ERR "Cannot create /proc/%s\n", PROC_FILENAME);
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s created with permissions 0444\n", PROC_FILENAME);
    return 0;
}

static void __exit proc_module_exit(void)
{
    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_INFO "/proc/%s removed\n", PROC_FILENAME);
}

module_init(proc_module_init);
module_exit(proc_module_exit);