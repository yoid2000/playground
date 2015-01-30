#include <stdint.h>

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

