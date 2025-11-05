#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Valeriya Kedank, Group 9-2");
MODULE_DESCRIPTION("/proc interface module with read/write capability");
MODULE_VERSION("1.0");

#define PROC_NAME "my_config"
#define MAX_SIZE 256

static struct proc_dir_entry *proc_file;
static char config_data[MAX_SIZE] = "default";
static DEFINE_MUTEX(config_mutex);

static ssize_t proc_read(struct file *file, char __user *user_buf, 
                        size_t count, loff_t *ppos)
{
    int len;
    char buffer[MAX_SIZE];
    
    if (*ppos > 0)
        return 0;
        
    mutex_lock(&config_mutex);
    len = snprintf(buffer, sizeof(buffer), "%s\n", config_data);
    mutex_unlock(&config_mutex);
    
    if (copy_to_user(user_buf, buffer, len)) {
        return -EFAULT;
    }
    
    *ppos = len;
    return len;
}

static ssize_t proc_write(struct file *file, const char __user *user_buf,
                         size_t count, loff_t *ppos)
{
    int ret;
    char temp_buf[MAX_SIZE];
    
    if (count >= MAX_SIZE) {
        printk(KERN_ERR "proc_module: Input too long (max %d characters)\n", MAX_SIZE-1);
        return -EINVAL;
    }
    
    memset(temp_buf, 0, sizeof(temp_buf));
    
    if (copy_from_user(temp_buf, user_buf, count)) {
        printk(KERN_ERR "proc_module: Failed to copy data from user\n");
        return -EFAULT;
    }
    
    // Remove newline if present
    if (temp_buf[count-1] == '\n')
        temp_buf[count-1] = '\0';
    else
        temp_buf[count] = '\0';
    
    mutex_lock(&config_mutex);
    strncpy(config_data, temp_buf, MAX_SIZE-1);
    config_data[MAX_SIZE-1] = '\0'; // Ensure null termination
    mutex_unlock(&config_mutex);
    
    printk(KERN_INFO "proc_module: Config updated to: %s\n", config_data);
    
    return count;
}

static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init proc_init(void)
{
    proc_file = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
    if (!proc_file) {
        printk(KERN_ERR "proc_module: Failed to create /proc/%s\n", PROC_NAME);
        return -ENOMEM;
    }
    
    printk(KERN_INFO "proc_module: Loaded successfully\n");
    printk(KERN_INFO "proc_module: Created /proc/%s with initial value: %s\n", 
           PROC_NAME, config_data);
    return 0;
}

static void __exit proc_exit(void)
{
    if (proc_file)
        proc_remove(proc_file);
        
    printk(KERN_INFO "proc_module: Unloaded successfully\n");
    printk(KERN_INFO "proc_module: Removed /proc/%s\n", PROC_NAME);
}

module_init(proc_init);
module_exit(proc_exit);