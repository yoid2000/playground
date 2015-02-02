#include <stdint.h>

#define HT_ATTACK 0
#define HT_ALL 1
#define HT_ATTACK_SUPPRESS_THRESH 5
#define HT_ALL_SUPPRESS_THRESH 10
#define HT_DECREMENT_TOUCHES_THRESH 100000
#define HT_NOISE_SD 1.0
// the following is actually doubled before first use
#define HT_INIT_DECREMENT_TOUCHES_THRESH 1000
#define HT_DECREMENT_TOUCHES_MAX 1000000
typedef struct high_touch_t {
  int touches;   // number of times user is non-overlap in attack setup
  int counts;    // number of times user is counted in any bucket
  int decrementThreshold;    // number of consecutive counts without
                             // without touch needed to decrement touch
			     // (doubles everytime  HT_ALL_SUPPRESS_THRESH
                             // is crossed)
} high_touch;

#define KEY_LEN 32
typedef struct hash_key_t {
  char str[KEY_LEN];
} hash_key;
