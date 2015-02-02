#include <stdint.h>

#define SIZE_CLOSE_THRESH 30

#define FILTER_LEN 16

/*
 * By right shifting the 10-bit bit number by 6, we get a
 * 4-bit index into the array of 64-bit filter words.
 */
#define FILTER_SHIFT 6
#define FILTER_MASK 0x3f

#define F_ONE_MAX 0x80
#define F_TWO_MAX (F_ONE_MAX<<2)
#define F_THREE_MAX (F_TWO_MAX<<2)
#define MAX_LEVEL 24  // absurdly large bucket (must be 24)

/*
 * level=0 means no filter
 * level=1 means all bucket entries are put into filter
 * level=2 means 1/4 the bucket entries are put into filter
 * level=3 means 1/16 the bucket entries are put into the filter
 * level=N means that 1/(2^^2(N-1)) bucket entries are put into the filter
 */
#define EMPTY_FILTER 0
typedef struct one_filter_t {
  uint8_t level;
  uint64_t filter[FILTER_LEN];  // 1024 bits wide.
} one_filter;

#define FILTERS_PER_BUCKET 3         // Must be 3!!!
/*
 * In a real system, the filters and bucket would not be in the same
 * structure.  Rather, the bucket would be deleted after use, and
 * re-created with a re-query if it is needed again.
 */
typedef struct bucket_t {
  one_filter filters[FILTERS_PER_BUCKET];
  int bsize;	    // number of entries
  uint64_t *list;   // pointer to first entry
  int sorted;	    // 1 if the user list has been sorted
} bucket;

typedef struct compare_t {
  int common;		// bits common between both filters
  int first;		// total bits the first (smaller) filter has
  int second;		// total bits the second (larger) filter has
  int numFirst;		// number of entries in first (smaller) bucket
  int numSecond;	// number of entries in second (larger) bucket
  int level;		// the level used for the comparison
			// 0 means no comparison made
  int index1;
  int index2;
  int overlap;
} compare;

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
