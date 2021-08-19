#include "scull.h"
MODULE_LICENSE("GPL");

static int err = 0;
struct scull_dev *devs;
static int scull_major = 0, scull_minor = SCULL_MINOR;
static ssize_t scull_qset_size = SCULL_QSET_SIZE, scull_quantum = SCULL_QUANTUM;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

int scull_trim(scull_dev *dev) {
  int i;
  scull_qset *cur = dev->qset, *next = 0;
  if (!dev) {
    return 0;
  }
  while (cur) {
    if (cur->data) {
      for (i = 0; i < dev->qset_size; ++i) {
        kfree(cur->data[i]);
      }
      kfree(cur->data);
      cur->data = 0;
    }
    next = cur->next;
    kfree(cur);
    cur = next;
  }
  dev->qset = 0;
  dev->size = 0;
  dev->qset_size = scull_qset_size;
  dev->quantum = scull_quantum;

  return 0;
}
static void *scull_seq_start(struct seq_file *s, loff_t *pos) {
  if (*pos >= SCULL_DEVS) {
    return 0;
  }
  return &devs[*pos];
}
static void scull_seq_stop(struct seq_file *s, void *v) {}
static void *scull_seq_next(struct seq_file *s, void *v, loff_t *pos) {
  (*pos)++;
  if (*pos >= SCULL_DEVS) {
    return 0;
  }
  return &devs[*pos];
}

static int scull_seq_show(struct seq_file *s, void *v) {
  scull_dev *s_dev = (scull_dev *)v;
  scull_qset *cur = 0;
  int i = 0;
  if (mutex_lock_interruptible(&s_dev->lock)) {
    return -ERESTARTSYS;
  }
  seq_printf(
      s, "Scull dev %ld: qset_size %ld, quantum %ld, size %ld, qset_ptr %p\n",
      (ssize_t)(s_dev - devs), s_dev->qset_size, s_dev->quantum, s_dev->size,
      s_dev->qset);
  cur = s_dev->qset;
  while (cur) {
    seq_printf(s, "qset_ptr: %p\n", cur);
    if (cur->data) {
      for (i = 0; i < s_dev->qset_size; ++i) {
        if (cur->data[i]) {
          seq_printf(s, "i: %i, cur->data[i]: %p:\n", i, cur->data[i]);
        }
      }
    }
    cur = cur->next;
  }
  mutex_unlock(&s_dev->lock);
  return 0;
}

static struct seq_operations scull_proc_sops = {
    .start = scull_seq_start,
    .next = scull_seq_next,
    .stop = scull_seq_stop,
    .show = scull_seq_show,
};
static int scull_seq_open(struct inode *inode, struct file *file) {
  return seq_open(file, &scull_proc_sops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops scull_proc_fops = {
    .proc_open = scull_seq_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,

};
#else
static struct file_operations scull_proc_fops = {
    .owner = THIS_MODULE,
    .open = scull_seq_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

#endif

int scull_open(struct inode *inode, struct file *filp) {
  struct scull_dev *s_dev;
  s_dev = container_of(inode->i_cdev, scull_dev, cdev);
  filp->private_data = s_dev;
  if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
    if (mutex_lock_interruptible(&s_dev->lock)) {
      return -ERESTARTSYS;
    }
    scull_trim(s_dev);
    mutex_unlock(&s_dev->lock);
  }
  return 0;
}

int scull_release(struct inode *inode, struct file *filp) { return 0; }
scull_qset *scull_follow(scull_dev *s_dev, ssize_t qsetll_off) {
  scull_qset *cur = s_dev->qset;
  if (!cur) {
    if (!(s_dev->qset = cur = kmalloc(sizeof(scull_qset), GFP_KERNEL))) {
      return 0;
    }
    memset(cur, 0, sizeof(scull_qset));
  }
  while (qsetll_off--) {
    if (!cur->next) {
      if (!(cur->next = kmalloc(sizeof(scull_qset), GFP_KERNEL))) {
        return 0;
      }
      memset(cur->next, 0, sizeof(scull_qset));
    }
    cur = cur->next;
  }

  return cur;
}
ssize_t scull_read(struct file *filp, char __user *buff, size_t count,
                   loff_t *offp) {
  ssize_t ret = 0;
  scull_dev *s_dev = filp->private_data;
  scull_qset *cur = 0;
  if (mutex_lock_interruptible(&s_dev->lock)) {
    return -ERESTARTSYS;
  }
  ssize_t quantum = s_dev->quantum, qset_size = s_dev->qset_size;
  ssize_t num_bytes = quantum * qset_size, qsetll_off = *offp / num_bytes,
          qset_tot = *offp % num_bytes, qset_off = qset_tot / quantum,
          byte_off = qset_tot % quantum;
  PDEBUG("offp: %lld count: %lu num_bytes: %ld qsetll_off: %ld qset_off: %ld "
         "byte_off: %ld",
         *offp, count, num_bytes, qsetll_off, qset_off, byte_off);
  if (*offp >= s_dev->size) {
    goto scull_read_end;
  }
  if (*offp + count > s_dev->size) {
    count = s_dev->size - *offp;
  }
  cur = scull_follow(s_dev, qsetll_off);
  if (!cur || !cur->data || !cur->data[qset_off]) {
    goto scull_read_end;
  }
  if (count > quantum - byte_off) {
    count = quantum - byte_off;
  }
  if (copy_to_user(buff, cur->data[qset_off] + byte_off, count)) {
    ret = -EFAULT;
    goto scull_read_end;
  }
  *offp += count;
  ret = count;
scull_read_end:
  mutex_unlock(&s_dev->lock);
  return ret;
}
ssize_t scull_write(struct file *filp, const char __user *buff, size_t count,
                    loff_t *offp) {
  ssize_t ret = -ENOMEM;
  scull_dev *s_dev = filp->private_data;
  scull_qset *cur = 0;
  if (mutex_lock_interruptible(&s_dev->lock)) {
    return -ERESTARTSYS;
  }
  ssize_t quantum = s_dev->quantum, qset_size = s_dev->qset_size;
  ssize_t num_bytes = quantum * qset_size, qsetll_off = *offp / num_bytes,
          qset_tot = *offp % num_bytes, qset_off = qset_tot / quantum,
          byte_off = qset_tot % quantum;
  PDEBUG("offp: %lld count: %lu num_bytes: %ld qsetll_off: %ld qset_off: %ld "
         "byte_off: %ld",
         *offp, count, num_bytes, qsetll_off, qset_off, byte_off);
  cur = scull_follow(s_dev, qsetll_off);
  if (!cur) {
    goto scull_write_end;
  }
  if (!cur->data) {
    if (!(cur->data = kmalloc(qset_size * sizeof(char *), GFP_KERNEL))) {
      goto scull_write_end;
    }
    memset(cur->data, 0, qset_size * sizeof(char *));
  }
  if (!cur->data[qset_off]) {
    if (!(cur->data[qset_off] = kmalloc(quantum, GFP_KERNEL))) {
      goto scull_write_end;
    }
  }
  if (count > quantum - byte_off) {
    count = quantum - byte_off;
  }
  if (copy_from_user(cur->data[qset_off] + byte_off, buff, count)) {
    ret = -EFAULT;
    goto scull_write_end;
  }
  *offp += count;
  if (s_dev->size < *offp) {
    s_dev->size = *offp;
  }
  ret = count;
scull_write_end:
  mutex_unlock(&s_dev->lock);
  return ret;
}
long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
  int ret = 0, err = 0;
  ssize_t tmp = 0;
  if ((_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) || (_IOC_NR(cmd) > SCULL_IOC_MAXNR)) {
    return -ENOTTY;
  }
  if ((_IOC_DIR(cmd) & _IOC_READ) || (_IOC_DIR(cmd) & _IOC_WRITE)) {
    err |= (!access_ok((void __user *)arg, _IOC_SIZE(cmd)));
  }
  if (err) {
    return -EFAULT;
  }
  switch (cmd) {
  case SCULL_IOCRESET:
    scull_qset_size = SCULL_QSET_SIZE;
    scull_quantum = SCULL_QUANTUM;
    break;
  case SCULL_IOCSQUANTUM:
    is_root();
    ret = __get_user(scull_quantum, (ssize_t __user *)arg);
    break;
  case SCULL_IOCTQUANTUM:
    is_root();
    scull_quantum = arg;
    break;
  case SCULL_IOCGQUANTUM:
    ret = __put_user(scull_quantum, (ssize_t __user *)arg);
    break;
  case SCULL_IOCQQUANTUM:
    return scull_quantum;
    break;
  case SCULL_IOCXQUANTUM:
    is_root();
    tmp = scull_quantum;
    ret = __get_user(scull_quantum, (ssize_t __user *)arg);
    if (!ret) {
      ret = __put_user(tmp, (ssize_t __user *)arg);
    }
    break;
  case SCULL_IOCHQUANTUM:
    is_root();
    tmp = scull_quantum;
    scull_quantum = arg;
    return arg;
  case SCULL_IOCSQSET:
    is_root();
    ret = __get_user(scull_qset_size, (ssize_t __user *)arg);
    break;
  case SCULL_IOCTQSET:
    is_root();
    scull_qset_size = arg;
    break;
  case SCULL_IOCGQSET:
    ret = __put_user(scull_qset_size, (ssize_t __user *)arg);
    break;
  case SCULL_IOCQQSET:
    return scull_qset_size;
    break;
  case SCULL_IOCXQSET:
    is_root();
    tmp = scull_qset_size;
    ret = __get_user(scull_qset_size, (ssize_t __user *)arg);
    if (!ret) {
      ret = __put_user(tmp, (ssize_t __user *)arg);
    }
    break;
  case SCULL_IOCHQSET:
    is_root();
    tmp = scull_qset_size;
    scull_qset_size = arg;
    return arg;
  }
  return 0;
}
struct file_operations scull_fops = {.owner = THIS_MODULE,
                                     .open = scull_open,
                                     .release = scull_release,
                                     .read = scull_read,
                                     .write = scull_write,
                                     .unlocked_ioctl = scull_ioctl};

static void exit_module(void) {
  int i;
  dev_t dev = MKDEV(scull_major, scull_minor);
  if (devs) {
    for (i = 0; i < SCULL_DEVS; ++i) {
      scull_trim(&devs[i]);
      cdev_del(&devs[i].cdev);
    }
    remove_proc_entry("scull", 0);
    kfree(devs);
  }
  PDEBUG("exit module scull");
  unregister_chrdev_region(dev, SCULL_DEVS);
}

static int __init init(void) {
  int res = 0, i = 0;
  dev_t dev = 0;
  scull_dev *s_dev = 0;
  res = alloc_chrdev_region(&dev, scull_minor, SCULL_DEVS, "scull");

  scull_major = MAJOR(dev);
  if (res < 0) {
    printk(KERN_WARNING "cant't get major");
    return res;
  }
  if (!(devs = kmalloc(SCULL_DEVS * sizeof(scull_dev), GFP_KERNEL))) {
    res = -ENOMEM;
    goto end;
  }
  memset(devs, 0, SCULL_DEVS * sizeof(scull_dev));

  for (i = 0; i < SCULL_DEVS; ++i) {
    s_dev = &devs[i];

    s_dev->size = 0;
    s_dev->qset_size = scull_qset_size;
    s_dev->quantum = scull_quantum;
    mutex_init(&s_dev->lock);

    dev = MKDEV(scull_major, scull_minor + i);
    cdev_init(&s_dev->cdev, &scull_fops);
    err = cdev_add(&s_dev->cdev, dev, 1);
    s_dev->cdev.owner = THIS_MODULE;
    s_dev->cdev.ops = &scull_fops;
    if (err) {
      printk(KERN_NOTICE "Error %d adding cdev %d", err, i);
    }
    proc_create("scull", 0, NULL, &scull_proc_fops);
  }
  PDEBUG("init success");
  return 0;
end:
  PDEBUG("init failure");
  exit_module();
  return res;
}
module_init(init);
module_exit(exit_module);
