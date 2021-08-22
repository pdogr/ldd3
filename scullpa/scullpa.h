#include "common.h"
#define SCULLPA_DEVS 4
#define SCULLPA_MINOR 0
#define SCULLPA_QSET_SIZE 8  // qset is set of 8 QUANTA
#define SCULLPA_ORDER 2      // use 1<<2 continuous pages for a quantum
#define PAGESIZE 4096
struct qset {
  void **data;
  struct qset *next;
} qset;

typedef struct scullpa_dev {
  struct qset *qset;
  struct mutex lock;
  struct cdev cdev;
  ssize_t quantum;
  ssize_t qset_size;
  size_t size;

} scullpa_dev;
