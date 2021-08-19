#include "common.h"

#define SCULL_P_BUFSIZE 1024
#define SCULL_P_DEVS 4

typedef struct scull_pipe {
  wait_queue_head_t inq, outq;
  char *buffer;
  int buffersize;
  int r, w;
  int nreaders, nwriters;
  struct mutex lock;
  struct cdev cdev;
} scull_pipe;
