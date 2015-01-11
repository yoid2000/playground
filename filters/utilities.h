#include <stdint.h>

typedef struct bucket_t {
  int bsize;	    // number of entries
  uint64_t *list;   // pointer to first entry
} bucket;
