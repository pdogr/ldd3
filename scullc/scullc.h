#include "common.h"
#define SCULLC_DEVS 4
#define SCULLC_MINOR 0
#define SCULLC_QSET_SIZE 1024  // qset is set of 1024 QUANTA
#define SCULLC_QUANTUM 4096    // quantum is a 4096 byte memory area

struct qset {
  void **data;
  struct qset *next;
} qset;

typedef struct scullc_dev {
  struct qset *qset;
  struct mutex lock;
  struct cdev cdev;
  ssize_t quantum;
  ssize_t qset_size;
  size_t size;

} scullc_dev;
