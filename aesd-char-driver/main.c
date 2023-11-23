/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"

#define HISTORY_BUFFER_SIZE 10
#define MAX_WRITE_COMMAND_SIZE 4096

// Define a structure to represent each write command
struct write_command {
    char *data;             // Pointer to the allocated memory for command content
    size_t size;            // Size of the allocated memory
};

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Youssef"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
struct write_command history_buffer[HISTORY_BUFFER_SIZE];  // History buffer for the most recent 10 write commands
int history_index = 0;  // Index to keep track of the next available slot in the history buffer

static DEFINE_MUTEX(aesd_mutex);

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
     
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    // Lock to ensure safe access to the history buffer
    mutex_lock(&aesd_mutex);

// Calculate the total size of the content in the history buffer
    size_t total_size = 0;
    for (int i = 0; i < HISTORY_BUFFER_SIZE; ++i) {
        total_size += history_buffer[i].size;
    }

    // Allocate a temporary buffer to hold the concatenated content
    char *temp_buffer = kmalloc(total_size, GFP_KERNEL);
    if (!temp_buffer) {
        retval = -ENOMEM;
        goto out;
    }

    // Copy the content of each write command to the temporary buffer
    size_t offset = 0;
    for (int i = 0; i < HISTORY_BUFFER_SIZE; ++i) {
        if (history_buffer[i].data) {
            size_t copy_size = min(count - offset, history_buffer[i].size);
            if (copy_to_user(buf + offset, history_buffer[i].data, copy_size)) {
                retval = -EFAULT;
                goto free_temp_buffer;
            }
            offset += copy_size;
        }
    }

    // Update the file position
    *f_pos += offset;

    // Set the return value to the number of bytes read
    retval = offset;

free_temp_buffer:
    kfree(temp_buffer);

out:
    mutex_unlock(&aesd_mutex);
    
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
     // Lock to ensure safe access to the history buffer
    mutex_lock(&aesd_mutex);

    // Allocate memory for the write command
    char *data = kmalloc(count, GFP_KERNEL);
    if (!data) {
        retval = -ENOMEM;
        goto out;
    }

    // Copy the data from user space
    if (copy_from_user(data, buf, count)) {
        retval = -EFAULT;
        goto free_data;
    }

    // Find the index for the next available slot in the history buffer
    int index = history_index % HISTORY_BUFFER_SIZE;

    // Free memory associated with the write command at the current index
    kfree(history_buffer[index].data);

    // Save the new write command in the history buffer
    history_buffer[index].data = data;
    history_buffer[index].size = count;

    // Move to the next available slot in the history buffer
    history_index++;

    // Set the return value to the number of bytes written
    retval = count;

free_data:
    if (retval < 0) {
        kfree(data);
    }

out:
    // Unlock the mutex before returning
    mutex_unlock(&aesd_mutex);

    // Unlock the mutex before returning
    mutex_unlock(&aesd_mutex);

    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    /**
     * TODO: initialize the AESD specific portion of the device
     */
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));
    
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
     

    // Lock to ensure safe access to the history buffer during cleanup
    mutex_lock(&aesd_mutex);

    // Logic to free memory associated with write commands more than 10 writes ago
    for (int i = 0; i < HISTORY_BUFFER_SIZE; ++i) {
        kfree(history_buffer[i].data);
    }

    // Unlock the mutex before unregistering the device
    mutex_unlock(&aesd_mutex);

    unregister_chrdev_region(devno, 1);

}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
