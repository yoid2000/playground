#include <stdint.h>
#include <sys/queue.h>
#include "./utilities.h"

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
#define MAX_LEVEL 11  // absurdly large bucket (must be 11)

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
#define MAX_CHILDREN 20
/*
 * In a real system, the filters and bucket would not be in the same
 * structure.  Rather, the bucket would be deleted after use, and
 * re-created with a re-query if it is needed again.
 */
typedef struct bucket_t {
  one_filter filters[FILTERS_PER_BUCKET];
  int bsize;	    // number of entries
  unsigned int *list;   // pointer to first entry (user)
  int sorted;	    // 1 if the user list has been sorted
  unsigned int children[MAX_CHILDREN];    // bucket index number
  int numChildren;
  void *clusterHead;
  LIST_ENTRY(bucket_t) entry;
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

typedef struct child_comb_t {
  unsigned int maxComb;
  unsigned int mask;
  bucket *cbp;
} child_comb;

typedef struct hammer_list_entry_t {
  bucket *head_bp;
  bucket *handle_bp;
  LIST_ENTRY(hammer_list_entry_t) entry;
} hammer_list_entry;

#define WITHOUT_CLUSTER_FREE 0
#define WITH_CLUSTER_FREE 1
#define CLUSTER_OVERLAP_HISTOGRAM 10
typedef struct cluster_t {
  LIST_HEAD(clusterListHead, bucket_t) head;  // list of buckets
  LIST_ENTRY(cluster_t) entry;   // for use with allClustersList
  int num;                       // number of buckets
  int histogram[CLUSTER_OVERLAP_HISTOGRAM];   // stats about overlap
} cluster;

typedef struct cluster_stats_t {
  int numClusters;
  mystats sizeS;
  int totalClusterOverlap;
  mystats overlapS;
  mystats histS[CLUSTER_OVERLAP_HISTOGRAM];
} cluster_stats;

// return values for addToCluster()
#define CLUSTER_ADD_NO_JOIN 0
#define CLUSTER_ADD_JOIN 1

/*
 *  The following used by initClusterNearMatches() and
 *  getNextClusterNearMatch();
 */
#define CLUSTER_CLOSE_SIZE 5
// quit after this many near-match attack clusters
#define MAX_NM_CLUSTERS 50
// only look for attack clusters with up to this many buckets per side
#define MAX_ATTACK_SIDE 4
// only examine clusters with this many or fewer buckets
#define MAX_CLUSTER_SIZE 10
#define MIN_CLUSTER_SIZE 4  // anything less caught by other mechanisms
#define CLUSTER_TOO_SMALL (MAX_NM_CLUSTERS+3)
#define CLUSTER_TOO_BIG (MAX_NM_CLUSTERS+2)
#define FOUND_MAX_ATTACK_CLUSTERS (MAX_NM_CLUSTERS+1)
