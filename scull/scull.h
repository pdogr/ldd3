#include "common.h"
#define SCULL_MINOR 0
#define SCULL_DEVS 4
#define SCULL_QSET_SIZE 1024  // qset is set of 1024 QUANTA
#define SCULL_QUANTUM 4096    // quantum is a 4096 byte memory area

#define SCULL_IOC_MAGIC 'p'
// Read/Write wrt application write -> copy_from_user

#define SCULL_IOCRESET _IO(SCULL_IOC_MAGIC, 0)
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC, 1, ssize_t)
#define SCULL_IOCSQSET _IOW(SCULL_IOC_MAGIC, 2, ssize_t)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC, 3)
#define SCULL_IOCTQSET _IO(SCULL_IOC_MAGIC, 4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC, 5, ssize_t)
#define SCULL_IOCGQSET _IOR(SCULL_IOC_MAGIC, 6, ssize_t)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC, 7)
#define SCULL_IOCQQSET _IO(SCULL_IOC_MAGIC, 8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9, ssize_t)
#define SCULL_IOCXQSET _IOWR(SCULL_IOC_MAGIC, 10, ssize_t)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC, 11)
#define SCULL_IOCHQSET _IO(SCULL_IOC_MAGIC, 12)
#define SCULL_IOC_MAXNR 12

#define is_root()                \
  if (!capable(CAP_SYS_ADMIN)) { \
    return -EPERM;               \
  }

typedef struct scull_qset {
  void **data;
  struct scull_qset *next;
} scull_qset;

typedef struct scull_dev {
  struct scull_qset *qset;
  struct cdev cdev;
  struct mutex lock;
  ssize_t qset_size, size, quantum;
  uid_t owner;
  int count;
  spinlock_t owner_lock;
} scull_dev;
