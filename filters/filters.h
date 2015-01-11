#include <stdint.h>

#define FILTER_LEN 16
typedef struct filter_t {
  uint8_t type;	// power of 2
  uint64_t filter[FILTER_LEN];  // 1024 bits wide
} filter;
