#include <linux/init.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/printk.h>

#include <linux/proc_fs.h>  
#include <linux/uaccess.h>  
#include <linux/version.h> 

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include <linux/minmax.h>  
#endif 


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)  
#define HAVE_PROC_OPS  
#endif 

#define PROCFS_MAX_SIZE 256UL
#define PROCFS_NAME "my_config"
#define DEFAULT_CONTENT "default\n" 

static struct proc_dir_entry* my_config_proc_file;
static char procfs_buffer[PROCFS_MAX_SIZE];
static unsigned long procfs_buffer_size = 0;

static ssize_t procfs_read(struct file* filp, char __user *buffer, size_t len, loff_t *offset) {
    if(*offset || procfs_buffer_size == 0) {
        pr_info("procfs_read: END\n");
        *offset = 0;
        return 0;
    }

    procfs_buffer_size = min(procfs_buffer_size, len);

    if(copy_to_user(buffer, procfs_buffer, procfs_buffer_size)) {
        pr_info("error copying to user");
        return -EFAULT;
    }

    *offset += procfs_buffer_size;

    pr_info("procfs_read: read %lu bytes\n", procfs_buffer_size);

    return procfs_buffer_size;
}   

static ssize_t procfs_write(struct file *filp, const char __user *buffer, size_t len, loff_t* offset) {
    procfs_buffer_size = min(PROCFS_MAX_SIZE - 1, len);

    if(copy_from_user(procfs_buffer, buffer, procfs_buffer_size)){ 
        pr_info("error copying from user");
        return -EFAULT;
    }

    *offset += procfs_buffer_size;
    procfs_buffer[procfs_buffer_size] = '\0';
    
    pr_info("procfs_write: write %lu bytes\n", procfs_buffer_size);
    return procfs_buffer_size;
}

#ifdef HAVE_PROC_OPS

static struct proc_ops my_config_ops = {
    .proc_read = procfs_read,
    .proc_write = procfs_write,
};

#else 

static const struct file_operations my_config_ops = {  
    .read = procfs_read,
    .write = procfs_write,  
};

#endif

static int __init proc_module_init(void) {
    my_config_proc_file = proc_create(
        PROCFS_NAME, 
        0666, 
        NULL,
        &my_config_ops
    );

    if(my_config_proc_file == NULL) {
        pr_info("ERROR: Could not create /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }
   
    procfs_buffer_size = snprintf(procfs_buffer, sizeof(DEFAULT_CONTENT), DEFAULT_CONTENT);
    pr_info("/proc/%s created\n", PROCFS_NAME);

    return 0;
}

static void __exit proc_module_exit(void) {
    proc_remove(my_config_proc_file);

    pr_info("/proc/%s removed\n", PROCFS_NAME);
}

module_init(proc_module_init)
module_exit(proc_module_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nikita Lukyanov");
MODULE_DESCRIPTION("Proc module, variant 2");
