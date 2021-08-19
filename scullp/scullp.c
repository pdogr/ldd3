#include "scullp.h"
static int err = 0;
static int scull_p_bufsize = SCULL_P_BUFSIZE, scullp_minor = 0,
           scullp_major = 0;
static scull_pipe *pipes;

static int scullp_open(struct inode *inode, struct file *filp) {
  scull_pipe *pipe = container_of(inode->i_cdev, scull_pipe, cdev);
  filp->private_data = pipe;
  if (mutex_lock_interruptible(&pipe->lock)) {
    return -ERESTARTSYS;
  }
  if (!pipe->buffer) {
    pipe->buffer = kmalloc(scull_p_bufsize, GFP_KERNEL);
    if (!pipe->buffer) {
      mutex_unlock(&pipe->lock);
      return -ENOMEM;
    }
  }
  pipe->buffersize = scull_p_bufsize;
  pipe->r = pipe->w = 0;
  if (filp->f_mode & FMODE_READ) {
    pipe->nreaders++;
  }
  if (filp->f_mode & FMODE_WRITE) {
    pipe->nwriters++;
  }
  mutex_unlock(&pipe->lock);
  return nonseekable_open(inode, filp);
}
static int scullp_release(struct inode *inode, struct file *filp) {
  scull_pipe *pipe = filp->private_data;
  mutex_lock(&pipe->lock);
  if (filp->f_mode & FMODE_READ) {
    --pipe->nreaders;
  }
  if (filp->f_mode & FMODE_WRITE) {
    --pipe->nwriters;
  }
  if (!pipe->nwriters && !pipe->nreaders) {
    kfree(pipe->buffer);
    pipe->buffer = 0;
  }
  mutex_unlock(&pipe->lock);
  return 0;
}
static ssize_t scullp_read(struct file *filp, char __user *buf, size_t count,
                           loff_t *offp) {
  scull_pipe *pipe = filp->private_data;
  if (mutex_lock_interruptible(&pipe->lock)) {
    return -ERESTARTSYS;
  }
  while (pipe->r == pipe->w) {
    mutex_unlock(&pipe->lock);
    if (filp->f_flags & O_NONBLOCK) {
      return -EAGAIN;
    }
    PDEBUG("%s read: going to sleep\n", current->comm);
    if (wait_event_interruptible(pipe->inq, (pipe->r != pipe->w))) {
      return -ERESTARTSYS;
    }
    if (mutex_lock_interruptible(&pipe->lock)) {
      return -ERESTARTSYS;
    }
  }
  if (pipe->w > pipe->r) {
    count = min(count, (size_t)(pipe->w - pipe->r));
  } else {
    count = min(count, (size_t)(pipe->buffersize - pipe->r));
  }
  PDEBUG("%s Going to read %li bytes", current->comm, count);
  if (copy_to_user(buf, pipe->buffer + pipe->r, count)) {
    mutex_unlock(&pipe->lock);
    return -EFAULT;
  }
  pipe->r += count;
  if (pipe->r == pipe->buffersize) {
    pipe->r = 0;
  }
  mutex_unlock(&pipe->lock);
  wake_up_interruptible(&pipe->outq);
  return count;
}
static int scullp_freesize(scull_pipe *pipe) {
  if (((pipe->w + 1) % pipe->buffersize) == pipe->r) {
    return 0;
  }
  return (pipe->buffersize + pipe->r - pipe->w - 1) % pipe->buffersize;
}
static ssize_t scullp_write(struct file *filp, const char __user *buf,
                            size_t count, loff_t *offp) {
  scull_pipe *pipe = filp->private_data;
  if (mutex_lock_interruptible(&pipe->lock)) {
    return -ERESTARTSYS;
  }
  while (!scullp_freesize(pipe)) {
    mutex_unlock(&pipe->lock);
    DEFINE_WAIT(wait);
    if (filp->f_mode & O_NONBLOCK) {
      return -EAGAIN;
    }
    PDEBUG("%s write: going to sleep\n", current->comm);
    prepare_to_wait(&pipe->outq, &wait, TASK_INTERRUPTIBLE);
    if (!scullp_freesize(pipe)) {
      schedule();
    }
    finish_wait(&pipe->outq, &wait);
    if (signal_pending(current) || mutex_lock_interruptible(&pipe->lock)) {
      return -ERESTARTSYS;
    }
  }
  count = min(count, (size_t)scullp_freesize(pipe));
  if (pipe->w >= pipe->r) {
    count = min(count, (size_t)(pipe->buffersize - pipe->w));
  }
  if (copy_from_user(pipe->buffer + pipe->w, buf, count)) {
    mutex_unlock(&pipe->lock);
    return -ERESTARTSYS;
  }
  PDEBUG("%s Write %li bytes to loc %i from %p\n", current->comm, count,
         pipe->w, buf);
  pipe->w += count;
  if (pipe->w == pipe->buffersize) {
    pipe->w = 0;
  }
  mutex_unlock(&pipe->lock);
  wake_up_interruptible(&pipe->inq);
  return count;
}
struct file_operations scullp_fops = {
    .owner = THIS_MODULE,
    .open = scullp_open,
    .read = scullp_read,
    .release = scullp_release,
    .write = scullp_write,

};

static void exit_module(void) {
  int i;
  dev_t dev = MKDEV(scullp_major, scullp_minor);
  if (pipes) {
    for (i = 0; i < SCULL_P_DEVS; ++i) {
      cdev_del(&pipes[i].cdev);
    }
    kfree(pipes);
  }
  pipes = 0;
  PDEBUG("exit module scull");
  unregister_chrdev_region(dev, SCULL_P_DEVS);
}
static int __init init(void) {
  int res = 0, i = 0;
  dev_t dev = 0;
  scull_pipe *pipe = 0;
  res = alloc_chrdev_region(&dev, scullp_minor, SCULL_P_DEVS, "scullp");
  scullp_major = MAJOR(dev);
  if (!(pipes = kmalloc(SCULL_P_DEVS * sizeof(scull_pipe), GFP_KERNEL))) {
    res = -ENOMEM;
    goto end;
  }
  memset(pipes, 0, SCULL_P_DEVS * sizeof(scull_pipe));
  for (i = 0; i < SCULL_P_DEVS; ++i) {
    pipe = &pipes[i];
    init_waitqueue_head(&pipe->inq);
    init_waitqueue_head(&pipe->outq);
    mutex_init(&pipe->lock);

    dev = MKDEV(scullp_major, scullp_minor + i);
    cdev_init(&pipe->cdev, &scullp_fops);
    err = cdev_add(&pipe->cdev, dev, 1);
    pipe->cdev.owner = THIS_MODULE;
    pipe->cdev.ops = &scullp_fops;
    pipe->nreaders = 0;
    pipe->nwriters = 0;
    if (err) {
      printk(KERN_NOTICE "Error %d adding cdev %d", err, i);
    }
    /* proc_create("scull", 0, NULL, &scull_proc_fops); */
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
