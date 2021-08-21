#include "scullc.h"
MODULE_LICENSE("GPL");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

static int trim(scullc_dev *);
static int follow(scullc_dev *, struct qset **, int qsetll_off);

static int err;
static int major = 0, minor = 0, quantum = SCULLC_QUANTUM,
           qset_size = SCULLC_QSET_SIZE;
static scullc_dev *devs;
static struct kmem_cache *cache;

static void *scull_seq_start(struct seq_file *s, loff_t *pos) {
  if (*pos >= SCULLC_DEVS) {
    return 0;
  }
  return &devs[*pos];
}
static void scull_seq_stop(struct seq_file *s, void *v) {}
static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos) {
  (*pos)++;
  if (*pos >= SCULLC_DEVS) {
    return 0;
  }
  return &devs[*pos];
}
static int scull_seq_show(struct seq_file *s, void *v) {
  scullc_dev *dev = (scullc_dev *)v;
  struct qset *qset = 0;
  int i = 0;
  if (mutex_lock_interruptible(&dev->lock)) {
    return -ERESTARTSYS;
  }
  seq_printf(s, "dev %ld: qset_size %ld, quantum %ld, size %ld, qset_ptr %p\n",
             (ssize_t)(dev - devs), dev->qset_size, dev->quantum, dev->size,
             dev->qset);
  qset = dev->qset;
  while (qset) {
    seq_printf(s, "qset_ptr: %p\n", qset);
    if (qset->data) {
      for (i = 0; i < dev->qset_size; ++i) {
        if (qset->data[i]) {
          seq_printf(s, "i: %i, qset->data[i]: %p:\n", i, qset->data[i]);
        }
      }
    }
    qset = qset->next;
  }
  mutex_unlock(&dev->lock);
  return 0;
}

static struct seq_operations proc_sops = {
    .start = scull_seq_start,
    .next = scull_seq_next,
    .stop = scull_seq_stop,
    .show = scull_seq_show,
};

static int scull_seq_open(struct inode *inode, struct file *filp) {
  return seq_open(filp, &proc_sops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops proc_fops = {
    .proc_open = scull_seq_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,

};
#else
static struct file_operations proc_fops = {
    .owner = THIS_MODULE,
    .open = scull_seq_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

#endif

static int trim(scullc_dev *dev) {
  int i = 0;
  struct qset *cur, *tmp;
  if (!dev) {
    return 0;
  }
  cur = dev->qset;
  while (cur) {
    if (cur->data) {
      for (i = 0; i < dev->qset_size; ++i) {
        if (cur->data[i]) {
          kmem_cache_free(cache, cur->data[i]);
        }
      }
      kfree(cur->data);
      cur->data = NULL;
    }
    tmp = cur;
    cur = cur->next;
    kfree(tmp);
  }
  dev->qset_size = qset_size;
  dev->quantum = quantum;
  dev->size = 0;
  dev->qset = NULL;
  return 0;
}
static int follow(scullc_dev *dev, struct qset **qset, int qsetll_off) {
  struct qset *cur = 0;
  cur = dev->qset;
  if (!cur) {
    if (!(dev->qset = cur = kmalloc(sizeof(struct qset), GFP_KERNEL))) {
      PDEBUG("follow could not alloc head of ll");
      return -ENOMEM;
    }
    memset(cur, 0, sizeof(struct qset));
  }
  while (qsetll_off--) {
    if (!cur->next) {
      if (!(cur->next = kmalloc(sizeof(struct qset), GFP_KERNEL))) {
        PDEBUG("follow could not alloc cur->next");
        return -ENOMEM;
      }
      memset(cur->next, 0, sizeof(struct qset));
      PDEBUG("success alloc cur->next");
    }
    cur = cur->next;
  }
  *qset = cur;
  return 0;
}
int open(struct inode *inode, struct file *filp) {
  scullc_dev *dev;
  dev = container_of(inode->i_cdev, scullc_dev, cdev);
  filp->private_data = dev;
  PDEBUG("dev: %p", dev);
  if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
    if (mutex_lock_interruptible(&dev->lock)) {
      return -ERESTARTSYS;
    }
    trim(dev);
    mutex_unlock(&dev->lock);
  }
  return 0;
}
int release(struct inode *inode, struct file *filp) { return 0; }
ssize_t read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
  ssize_t r = 0, cquantum = 0, num_bytes = 0, qsetll_off = 0, qset_tot = 0,
          qset_off = 0, byte_off = 0;
  scullc_dev *dev = 0;
  struct qset *qset = 0;
  dev = filp->private_data;
  if (mutex_lock_interruptible(&dev->lock)) {
    PDEBUG("Unable to get lock");
    return -ERESTARTSYS;
  }
  cquantum = dev->quantum;
  num_bytes = cquantum * dev->qset_size, qsetll_off = *offp / num_bytes,
  qset_tot = *offp % num_bytes, qset_off = qset_tot / cquantum,
  byte_off = qset_tot % cquantum;
  if (*offp > dev->size) {
    goto read_end;
  }
  if (*offp + count > dev->size) {
    count = dev->size - *offp;
  }
  if (follow(dev, &qset, qsetll_off)) {
    PDEBUG("unable to follow no mem");
    r = -ENOMEM;
    goto read_end;
  }
  PDEBUG("moved to required qset %p", qset);
  if (!qset || !qset->data || !qset->data[qset_off]) {
    goto read_end;
  }
  if (count > cquantum - byte_off) {
    count = cquantum - byte_off;
  }
  if (copy_to_user(buff, qset->data[qset_off] + byte_off, count)) {
    PDEBUG("unable to copy_to_user");
    r = -EFAULT;
    goto read_end;
  }
  PDEBUG(
      "%s read: offp: %lld count: %lu num_bytes: %ld qsetll_off: %ld qset_off: "
      "%ld "
      "byte_off: %ld",
      current->comm, *offp, count, num_bytes, qsetll_off, qset_off, byte_off);
  *offp += count;
  mutex_unlock(&dev->lock);
  return count;
read_end:
  mutex_unlock(&dev->lock);
  return r;
}
ssize_t write(struct file *filp, const char __user *buff, size_t count,
              loff_t *offp) {
  ssize_t r = 0, cquantum = 0, num_bytes = 0, qsetll_off = 0, qset_tot = 0,
          qset_off = 0, byte_off = 0;
  scullc_dev *dev = 0;
  struct qset *qset = 0;
  dev = filp->private_data;
  if (mutex_lock_interruptible(&dev->lock)) {
    PDEBUG("unable to get lock");
    return -ERESTARTSYS;
  }
  cquantum = dev->quantum;
  num_bytes = cquantum * dev->qset_size, qsetll_off = *offp / num_bytes,
  qset_tot = *offp % num_bytes, qset_off = qset_tot / cquantum,
  byte_off = qset_tot % cquantum;
  if (follow(dev, &qset, qsetll_off)) {
    PDEBUG("unable to follow no mem");
    r = -ENOMEM;
    goto write_end;
  }
  PDEBUG("moved to required qset %p", qset);
  if (!qset->data) {
    if (!(qset->data = kmalloc(dev->qset_size * sizeof(void *), GFP_KERNEL))) {
      PDEBUG("no memory for qset");
      r = -ENOMEM;
      goto write_end;
    }
    memset(qset->data, 0, dev->qset_size * sizeof(void *));
    PDEBUG("succesfully alloc qset->data");
  }
  if (!qset->data[qset_off]) {
    if (!(qset->data[qset_off] = kmem_cache_alloc(cache, GFP_KERNEL))) {
      PDEBUG("no memoery for quanutm");
      r = -ENOMEM;
      goto write_end;
    }
    memset(qset->data[qset_off], 0, cquantum);
    PDEBUG("succesfully alloc qset->data[dqset_off]");
  }
  if (count > cquantum - byte_off) {
    count = cquantum - byte_off;
  }
  if (copy_from_user(qset->data[qset_off] + byte_off, buff, count)) {
    PDEBUG("error in copy_from_user");
    r = -EFAULT;
    goto write_end;
  }
  PDEBUG(
      "%s write: offp: %lld count: %lu num_bytes: %ld qsetll_off: %ld "
      "qset_off: "
      "%ld "
      "byte_off: %ld",
      current->comm, *offp, count, num_bytes, qsetll_off, qset_off, byte_off);
  *offp += count;
  if (dev->size < *offp) {
    dev->size = *offp;
  }
  mutex_unlock(&dev->lock);
  return count;

write_end:
  mutex_unlock(&dev->lock);
  return r;
}

struct file_operations fops = {.owner = THIS_MODULE,
                               .open = open,
                               .release = release,
                               .read = read,
                               .write = write};

static void exit_m(void) {
  int i;
  if (devs) {
    for (i = 0; i < SCULLC_DEVS; ++i) {
      cdev_del(&devs[i].cdev);
      trim(&devs[i]);
    }
    kfree(devs);
    devs = 0;
  }
  if (cache) {
    kmem_cache_destroy(cache);
  }
  remove_proc_entry("scullc", 0);
  PDEBUG("scullc: exit module");
  unregister_chrdev_region(MKDEV(major, minor), SCULLC_DEVS);
}

static int __init init_m(void) {
  int r, i;
  dev_t _dev = 0;
  r = alloc_chrdev_region(&_dev, minor, SCULLC_DEVS, "scullc");
  major = MAJOR(_dev);
  if (r < 0) {
    return r;
  }
  if (!(devs = kmalloc(SCULLC_DEVS * sizeof(scullc_dev), GFP_KERNEL))) {
    r = -ENOMEM;
    goto init_end;
  }
  memset(devs, 0, SCULLC_DEVS * sizeof(scullc_dev));
  for (i = 0; i < SCULLC_DEVS; ++i) {
    scullc_dev *dev = &devs[i];
    dev->quantum = quantum;
    dev->qset_size = qset_size;
    dev->size = 0;
    mutex_init(&dev->lock);
    cdev_init(&dev->cdev, &fops);
    dev->cdev.owner = THIS_MODULE;
    if (cdev_add(&dev->cdev, MKDEV(major, i), 1)) {
      PDEBUG("Error %d adding cdev %d", err, i);
    }
  }
  cache = kmem_cache_create("scullc", quantum, 0, SLAB_HWCACHE_ALIGN, NULL);
  if (!cache) {
    exit_m();
    return -ENOMEM;
  }
  proc_create("scullc", 0, 0, &proc_fops);
  PDEBUG("scullc: init success");
  return 0;
init_end:
  PDEBUG("scullc: init Failure");
  unregister_chrdev_region(MKDEV(major, minor), SCULLC_DEVS);
  return r;
}

module_init(init_m);
module_exit(exit_m);
