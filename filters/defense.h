
// parameters for both attacks.c and defenses.c

// describing what defenses should be used.  These must be in
// increasing order as shown
#define NUM_DEFENSES 4
#define BASIC_DEFENSE 0
#define OtO_DEFENSE 1
#define MtO_DEFENSE 2
#define MtM_DEFENSE 3

// Where to place victim in order of attack buckets
#define NUM_ORDERS 3
#define VICTIM_FIRST 0
#define VICTIM_LAST 1
#define VICTIM_RANDOM 2

// Whether or not victim has attribute
#define NUM_ATTRIBUTES 2
#define VICTIM_ATTRIBUTE_YES 0
#define VICTIM_ATTRIBUTE_NO 1

// Type of attack
#define NUM_ATTACKS 3
#define OtO_ATTACK 0
#define MtO_ATTACK 1
#define MtM_ATTACK 2

// Subtype of attack
#define NUM_SUBTYPES 2
#define RANDOM_CHILDREN 0
#define SEGREGATED_CHILDREN 1

// For OtM, whether victim is in parent or one child or all children
#define NUM_VICTIM_LOC 3
#define VICTIM_IN_PARENT 0
#define VICTIM_IN_ONE_CHILD 1
#define VICTIM_IN_ALL_CHILDREN 2

// For MtM, the type of attack (mtmType)
#define NUM_MTM_TYPES 2
#define ZIG_ZAG 0
#define FULL_CLUSTER 1

typedef struct attack_setup_t {
  int attack;
  char *attack_str[NUM_ATTACKS];
  int subAttack;
  char *subAttack_str[NUM_ATTACKS];
  int defense;
  char *defense_str[NUM_DEFENSES];
  int order;
  char *order_str[NUM_ORDERS];
  int attribute;
  char *location_str[NUM_VICTIM_LOC];
  int location;
  char *attribute_str[NUM_ATTRIBUTES];
  int mtmType;
  char *mtmType_str[NUM_MTM_TYPES];
  int mtmNumBaseBlocks;    // number of blocks from which to build attack
                            // don't define if zig zag
  int mtmNumExtraBlocks;   // number of extra blocks on each side
  int mtmNumLeftBuckets;    // number of buckets on the "left" of cluster
  int mtmNumRightBuckets;    // number of buckets on the "right" of cluster
                            // don't define if zig zag
  int numChildren;   // for OtM attack
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

#define MAX_NUM_BLOCKS 16
#define MAX_NUM_BUCKETS_PER_SIDE 16
#define NO_BLOCK 0x7fffffff;

typedef struct mtm_bucket_t {
  int numBlocks;
  int blocks[MAX_NUM_BLOCKS];
} mtm_bucket;

#define LEFT 0
#define RIGHT 1
typedef struct mtm_cluster_t {
  int numBuckets[2];
  mtm_bucket bucket[2][MAX_NUM_BUCKETS_PER_SIDE];
  unsigned char strings[2][32];
} mtm_cluster;
