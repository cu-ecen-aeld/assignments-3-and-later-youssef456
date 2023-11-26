#ifndef AESD_IOCTL_H
#define AESD_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

// Define an arbitrary ioctl magic number
#define AESD_IOC_MAGIC 0x16

// Define a write command structure for seek ioctl
struct aesd_seekto {
    uint32_t write_cmd;
    uint32_t write_cmd_offset;
};

// Define the ioctl command for seek operation
#define AESDCHAR_IOCSEEKTO _IOWR(AESD_IOC_MAGIC, 1, struct aesd_seekto)

// Define the maximum number of ioctl commands for bounds checking
#define AESDCHAR_IOC_MAXNR 1

#endif /* AESD_IOCTL_H */
