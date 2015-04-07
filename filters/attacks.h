
// parameters for both attacks.c and defenses.c

// describing what defenses should be used.  These must be in
// increasing order as shown
#define NUM_DEFENSES 4
#define BASIC_DEFENSE 0
#define OtO_DEFENSE 1
#define MtO_DEFENSE 2
#define MtM_DEFENSE 3

// Where to place victim in order of attack buckets
#define NUM_ORDERS 2
#define VICTIM_FIRST 0
#define VICTIM_LAST 1

// Whether or not victim has attribute (must be these values)
#define NUM_ATTRIBUTES 2
#define VICTIM_ATTRIBUTE_YES 0
#define VICTIM_ATTRIBUTE_NO 1

// Subtype of attack
#define NUM_SUBTYPES 2
#define RANDOM_CHILDREN 0
#define SEGREGATED_CHILDREN 1

// as->location
#define NUM_VICTIM_LOC 6
#define VICTIM_IN_PARENT 0
#define VICTIM_IN_ONE_CHILD 1
#define VICTIM_IN_ALL_CHILDREN 2
#define VICTIM_IN_ALL_LEFT 3
#define VICTIM_IN_ALL_RIGHT 4
#define VICTIM_IN_ONE_RIGHT 5

// as->clusterType
#define NUM_CLUSTER_TYPES 5
#define GENERAL_CLUSTER 0
#define PERFECT_CLUSTER 1
#define BARBELL_CHAIN_CLUSTER 2
#define NUNCHUK_CLUSTER 3
#define NUNCHUK_BARBELL_CLUSTER 4

#define BARBELL_SIZE 15  // number of blocks in the big block group

typedef struct attack_setup_t {
  int subAttack;
  char *subAttack_str[NUM_SUBTYPES];
  int defense;
  char *defense_str[NUM_DEFENSES];
  int order;
  char *order_str[NUM_ORDERS];
  int location;
  char *location_str[NUM_VICTIM_LOC];
  int attribute;
  char *attribute_str[NUM_ATTRIBUTES];
  int clusterType;
  char *clusterType_str[NUM_CLUSTER_TYPES];
  int usersPerBucket;    // target number of users per bucket
  int numBaseBlocks;    // number of blocks *beyond the minimum* 
                           // from which to build attack
  int minLeftBuckets;   // min buckets on left side
  int maxLeftBuckets;   // max buckets on left side
  int numLeftBuckets;    // derived from min and max
  int minRightBuckets;   // min buckets on right side
  int maxRightBuckets;    // max buckets on right side 
  int numRightBuckets;    // derived from min and max
  int chaffMax;
  int chaffMin;
  int numRounds;  // number of attack repeats 
                  // (needed for statistical significance)
  int numSamples; // number of attack samples
  FILE *f;
} attack_setup;

// the routine that produces a block expects two values as input,
// which are historically called sample and child (based on initial
// MtO attack software).  This structure is used for MtM attacks.
typedef struct blocks_t {
  int sampleNum;
  int childNum;
} blocks;

#define MAX_NUM_BLOCKS 32
#define MAX_NUM_BUCKETS_PER_SIDE 32
#define NO_BLOCK 0x7fffffff;

typedef struct mtm_bucket_t {
  int numBlocks;
  int blocks[MAX_NUM_BLOCKS];
  bucket *bp;	         // place-holder for built bucket
} mtm_bucket;

#define LEFT 0       // must be zero!
#define RIGHT 1      // must be one!
typedef struct mtm_cluster_t {
  int numBuckets[2];
  mtm_bucket bucket[2][MAX_NUM_BUCKETS_PER_SIDE];
  unsigned char strings[2][32];
} mtm_cluster;
