
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
  int numChildren;   // for OtM attack
  int chaffMax;
  int chaffMin;
} attack_setup;
