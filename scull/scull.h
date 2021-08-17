#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include<linux/proc_fs.h>
#include<linux/version.h>

#define SCULL_MINOR 0
#define SCULL_DEVS 4
#define SCULL_QSET_SIZE 65536 // qset is set of 1024 QUANTA
#define SCULL_QUANTUM 65536   // quantum is a 1024 byte memory area

#undef PDEBUG
#ifdef SCULL_DEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...)                                \
 printk(KERN_DEBUG "scull: %s %d", __FUNCTION__, __LINE__); \
 printk(KERN_DEBUG "scull: " fmt, ##args)
#else
#define PDEBUG(fmt, args...)                              \
 fprintf(stderr, "scull: %s %d", __FUNCTION__, __LINE__); \
 fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) 
#endif

typedef struct scull_qset {
 void **data;
 struct scull_qset *next;
} scull_qset;

typedef struct scull_dev {
 struct scull_qset *qset;
 struct cdev cdev;
 struct mutex lock;
 ssize_t qset_size, size, quantum;
} scull_dev;

