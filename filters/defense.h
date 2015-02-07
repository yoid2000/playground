
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
#define NUM_ATTRIBUTES 3
#define VICTIM_ATTRIBUTE_YES 0
#define VICTIM_ATTRIBUTE_NO 1
#define VICTIM_ATTRIBUTE_RANDOM 2

// Type of attack
#define NUM_ATTACKS 3
#define OtO_ATTACK 0
#define MtO_ATTACK 1
#define MtM_ATTACK 2

typedef struct attack_setup_t {
  int attack;
  char *attack_str[NUM_ATTACKS];
  int defense;
  char *defense_str[NUM_DEFENSES];
  int order;
  char *order_str[NUM_ORDERS];
  int attribute;
  char *attribute_str[NUM_ATTRIBUTES];
} attack_setup;
