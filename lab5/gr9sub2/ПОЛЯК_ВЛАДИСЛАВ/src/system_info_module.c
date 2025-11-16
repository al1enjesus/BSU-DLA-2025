#include <linux/init.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>  
#include <linux/uaccess.h> 
#include <linux/version.h>
#include <linux/ktime.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)  
#define HAVE_PROC_OPS  
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include <linux/minmax.h>  
#endif 

#define MAX_OUTPUT_LENGTH 512UL
#define PROCFS_NAME "sys_stats"
static struct proc_dir_entry *sys_stats_proc_file;

static ssize_t procfs_read(struct file* filp, char __user *buffer, size_t len, loff_t *offset) {
    if(*offset) {
        pr_info("procfs_read: END\n");
        *offset = 0;
        return 0;
    }
    
#define K(x) ((x) << (PAGE_SHIFT - 10))
    struct sysinfo i;
    long available;

    si_meminfo(&i);
    available = si_mem_available();

    int count = 0;
    struct task_struct* task;
    for_each_process(task) {
        count++;
    }

    s64 uptime = ktime_to_ms(ktime_get_boottime()) / 1000ll;

    char buf[MAX_OUTPUT_LENGTH];
    unsigned long buf_size = snprintf(
        buf, 
        MAX_OUTPUT_LENGTH, 
        "Processes: %d\nMemory Used: %ld MB\nSystem Uptime %lld seconds\n",
        count, (K(i.totalram) - K(available)) / 1024, uptime
    );
    
    buf_size = min(buf_size, MAX_OUTPUT_LENGTH - 1);
    buf_size = min(buf_size, len);

    if(copy_to_user(buffer, buf, buf_size)) {
        pr_info("error copying to user");
        return -EFAULT;
    }

    *offset += buf_size;

    pr_info("procfs_read: read %lu bytes\n", buf_size);

    return buf_size;
} 

#ifdef HAVE_PROC_OPS

static struct proc_ops system_info_ops = {
    .proc_read = procfs_read,
};

#else 

static const struct file_operations system_info_ops = {  
    .read = procfs_read,  
};

#endif

static int __init proc_module_init(void) {
    sys_stats_proc_file = proc_create(
        PROCFS_NAME, 
        0, 
        NULL,
        &system_info_ops
    );

    if(sys_stats_proc_file == NULL) {
        pr_info("ERROR: Could not create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }
   
    pr_info("/proc/%s created\n", PROCFS_NAME);

    return 0;
}

static void __exit proc_module_exit(void) {
    proc_remove(sys_stats_proc_file);

    pr_info("/proc/%s removed\n", PROCFS_NAME);
}

module_init(proc_module_init)
module_exit(proc_module_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vladislav Paliak");
MODULE_DESCRIPTION("System Stats module, variant 2");
