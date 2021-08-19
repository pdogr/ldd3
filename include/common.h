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

