#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/version.h>

#undef PDEBUG
#ifdef SCULL_DEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...)                                 \
  printk(KERN_DEBUG "scull: %s %d", __FUNCTION__, __LINE__); \
  printk(KERN_DEBUG "scull: " fmt, ##args)
#else
#define PDEBUG(fmt, args...)                               \
  fprintf(stderr, "scull: %s %d", __FUNCTION__, __LINE__); \
  fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...)
#endif
