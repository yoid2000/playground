#include <stdio.h>
#include <time.h>
#include <string.h>
#include <search.h>
#include <stdlib.h>
#include "./filters.h"
#include "./hightouch.h"

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeBucket(int arg1);
extern int exceedsNoisyThreshold(float arg1, float arg2, float arg3);

high_touch *ht_table;
ENTRY *entry_table;
hash_key *keys_table;
int max_ht_table;

initHighTouch(high_touch *ht) {
  ht->touches = 0;
  ht->counts = 0;
  ht->decrementThreshold = HT_INIT_DECREMENT_TOUCHES_THRESH;
}

initHighTouchTable()
{
  int i;

  for (i = 0; i < max_ht_table; i++) {
    initHighTouch(&(ht_table[i]));
  }
}

countNumTouches(int *numTouches, int maxNumTouches)
{
  int i;

  for (i = 0; i < max_ht_table; i++) {
    if (ht_table[i].touches >= (maxNumTouches-1)) {
      numTouches[maxNumTouches-1]++;
    }
    else {
      numTouches[ht_table[i].touches]++;
    }
  }
}

/*
 * This should be run once for the duration of the simulation,
 * and not run again.  (There is no code to free this stuff.
 * We just let the simulation end.)
 */
bucket *
createHighTouchTable(int size)
{
  bucket *bp;
  unsigned int *lp;
  int i;
  ENTRY *ulookup;

  // make the high_touch entries table
  max_ht_table = size;
  ht_table = (high_touch *) calloc(size, sizeof(high_touch));
  entry_table = (ENTRY *) calloc(size, sizeof(ENTRY));
  keys_table = (hash_key *) calloc(size, sizeof(hash_key));
  initHighTouchTable();
  // generate a user id per table entry
  bp = makeRandomBucket(size);
  // build a hash table mapping user id into table entry
  if (hcreate(size*2) == 0) {
    printf("hcreate failed!!\n");
    exit(1);
  }
  lp = bp->list;
  for (i = 0; i < size; i++) {
    snprintf(keys_table[i].str, KEY_LEN, "%d", *lp);
    entry_table[i].key = keys_table[i].str;
    entry_table[i].data = (void *) &(ht_table[i]);
    if (hsearch(entry_table[i], ENTER) == NULL) {
      printf("hsearch failed!\n");
      exit(1);
    }
    lp++;
  }
  //printBucket(bp);
  return(bp);
}

high_touch *
getHighTouch(unsigned int uid)
{
  ENTRY *uhash, ulookup;
  high_touch *ht;
  char uids[KEY_LEN];

  snprintf(uids, KEY_LEN, "%d", uid);
  ulookup.key = uids;
  ulookup.data = (void *) NULL;

  if ((uhash = hsearch(ulookup, FIND)) == NULL) {
    printf("getHighTouch: hsearch failed (uid %x, key %s)!!\n", 
             uid, ulookup.key);
    exit(1);
  }

  ht = (high_touch *) uhash->data;
  //printf("getHighTouch: good hsearch (uid %x, key %s!!\n", uid, ulookup.key);
  
  return(ht);
}

/*
 *  Called when user is non-overlap in an attack setup
 */
touchUser(unsigned int uid)
{
  high_touch *ht;

  ht = getHighTouch(uid);

  ht->touches++;
  // the user is touched, so restart the decrement counter
  ht->counts = 0;
  // and double the threshold to make it harder to continue to attack
  // this user
  ht->decrementThreshold *= 2;
  if (ht->decrementThreshold > HT_DECREMENT_TOUCHES_MAX) {
    ht->decrementThreshold = HT_DECREMENT_TOUCHES_MAX;
  }
}

int
isUserSuppress(unsigned int uid, int threshold)
{
  high_touch *ht;
  int suppress = 0;    // assume won't suppress

  ht = getHighTouch(uid);

  if (exceedsNoisyThreshold(threshold, ht->touches, HT_NOISE_SD)) {
    suppress = 1;
  }
  return(suppress);
}

/*
 *  Return 1 if user should be suppressed, 0 otherwise
 *  Assumes that this is called because user placed in bucket,
 *  so also increment counts and see if touches should be decremented.
 */
int
isUserSuppressAll(int uid)
{
  return(isUserSuppress(uid, HT_ALL_SUPPRESS_THRESH));
}

/*
 *  Return 1 if user should be suppressed, 0 otherwise
 */
int
isUserSuppressAttack(int uid)
{
  return(isUserSuppress(uid, HT_ATTACK_SUPPRESS_THRESH));
}

touchNonOverlappingUsers(bucket *bp)
{
  int i;
  unsigned int *lp;

  lp = bp->list;
  for (i = 0; i < bp->bsize; i++) {
    touchUser(*lp);
    lp++;
  }
}

countUsers(bucket *bp)
{
  int i;
  unsigned int *lp;
  high_touch *ht;

  lp = bp->list;
  for (i = 0; i < bp->bsize; i++) {
    ht = getHighTouch(*lp);
    ht->counts++;
    if (ht->counts > ht->decrementThreshold) {
      // this user hasn't been touched in a while
      ht->counts = 0;
      if (ht->touches > 0) {ht->touches--;}
    }
    lp++;
  }
}

/* TESTS */

test_createHighTouchTable()
{
  int i;
  high_touch *ht;
  bucket *bp;

  bp = createHighTouchTable(1000);
  
  for (i = 0; i < 1000; i++) {
    ht = getHighTouch((unsigned int) bp->list[i]);
  }
  for (i = 0; i < 1000; i++) {
    ht = getHighTouch(bp->list[i]);
  }
  printf("test_createHighTouchTable() passed\n");
}

test_hsearch()
{
  ENTRY *uhash, ulookup;
  unsigned char *str = "here is the entry\n";
  char thekey[KEY_LEN];

  if (hcreate(1000) == 0) {
    printf("createHashTable failed!\n");
    exit(1);
  }

  ulookup.data = (void *) str;
  snprintf(thekey, KEY_LEN, "%d", 0x12345678);
  ulookup.key = thekey;

  if ((uhash = hsearch(ulookup, FIND)) != NULL) {
    printf("test_hsearch() FAILED F1\n"); exit(1);
  }
  if ((uhash = hsearch(ulookup, ENTER)) == NULL) {
    printf("test_hsearch() FAILED F2\n"); exit(1);
  }
  if ((uhash = hsearch(ulookup, FIND)) == NULL) {
    printf("test_hsearch() FAILED F5\n"); exit(1);
  }
  if (strcmp(ulookup.key, uhash->key) != 0) {
    printf("test_hsearch() FAILED F3\n"); exit(1);
  }
  if (uhash == &ulookup) {
    printf("test_hsearch() FAILED F4\n"); exit(1);
  }

  hdestroy();
  printf("test_hsearch() passed.\n");
}

do_highTouch(unsigned int u_id)
{
  int i;
  high_touch *ht;

  ht = getHighTouch(u_id);

  if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 1) {
    printf("F1: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->touches != 0) {
    printf("F2: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->counts != 1) {
    printf("F3: test_highTouch() FAILED\n"); exit(1);
  }
  touchUser(u_id);
  if (ht->touches != 1) {
    printf("F4: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->counts != 0) {
    printf("F5: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->decrementThreshold != (2 * HT_INIT_DECREMENT_TOUCHES_THRESH)) {
    printf("F8: test_highTouch() FAILED (%d)\n", ht->decrementThreshold); exit(1);
  }
  if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 1) {
    printf("F6: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->counts != 1) {
    printf("F7: test_highTouch() FAILED\n"); exit(1);
  }
  for (i = 0; i < 500; i++) {
    if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 1) {
      printf("F9: test_highTouch() FAILED (%d)\n", i); exit(1);
    }
    if (ht->counts != (i+2)) {
      printf("F10: test_highTouch() FAILED (%d, %d)\n", ht->counts, i+2); exit(1);
    }
  }
  for (i = 0; i < 1000; i++) {
    if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 1) {
      printf("F11: test_highTouch() FAILED (%d)\n", i); exit(1);
    }
    if (ht->counts != (i+2+500)) {
      printf("F12: test_highTouch() FAILED (%d, %d)\n", ht->counts, i+2+500); exit(1);
    }
  }
  for (i = 0; i < 1000; i++) {
    if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 1) {
      printf("F24: test_highTouch() FAILED (%d)\n", i); exit(1);
    }
  }
  // should have pushed touches back to 0 cause of all the isUserSuppress()
  if (ht->touches != 0) {
    printf("F13: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->decrementThreshold != (2 * HT_INIT_DECREMENT_TOUCHES_THRESH)) {
    printf("F14: test_highTouch() FAILED (%d)\n", ht->decrementThreshold); exit(1);
  }
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  if (ht->counts != 0) {
    printf("F15: test_highTouch() FAILED\n"); exit(1);
  }
  if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 0) {
    printf("F16: test_highTouch() FAILED\n"); exit(1);
  }
  if (isUserSuppress(u_id, HT_ALL_SUPPRESS_THRESH) == 1) {
    printf("F17: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->decrementThreshold != (256 * HT_INIT_DECREMENT_TOUCHES_THRESH)) {
    printf("F18: test_highTouch() FAILED (%d)\n", ht->decrementThreshold); exit(1);
  }
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  touchUser(u_id);
  if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 0) {
    printf("F19: test_highTouch() FAILED\n"); exit(1);
  }
  if (isUserSuppress(u_id, HT_ALL_SUPPRESS_THRESH) == 0) {
    printf("F20: test_highTouch() FAILED\n"); exit(1);
  }
  if (ht->decrementThreshold > HT_DECREMENT_TOUCHES_MAX) {
    printf("F21: test_highTouch() FAILED (%d)\n", ht->decrementThreshold); exit(1);
  }
  for (i = 0; i < (HT_DECREMENT_TOUCHES_MAX*7)+100; i++) {
    isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH);
  }
  if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 0) {
    printf("F22: test_highTouch() FAILED\n"); exit(1);
  }
  for (i = 0; i < (HT_DECREMENT_TOUCHES_MAX*7)+100; i++) {
    isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH);
  }
  if (isUserSuppress(u_id, HT_ATTACK_SUPPRESS_THRESH) == 1) {
    printf("F23: test_highTouch() FAILED\n"); exit(1);
  }
  printf("test_highTouch() passed\n"); exit(1);
}

// don't run both test_hsearch and test_highTouch in the same program
test_highTouch()
{
  int i;
  high_touch *ht;
  bucket *bp;

  bp = createHighTouchTable(1000);

  for (i = 0; i < 1000; i++) {
    ht = getHighTouch(bp->list[i]);
  }
  for (i = 0; i < 1000; i++) {
    ht = getHighTouch(bp->list[i]);
  }

  ht = getHighTouch(bp->list[1]);
  do_highTouch(bp->list[1]);
  ht = getHighTouch(bp->list[0]);
  do_highTouch(bp->list[0]);
  for (i = 2; i < 500; i++) {
    touchUser(bp->list[i]);
    ht = getHighTouch(bp->list[i]);
    if (ht->touches != 1) {
      printf("F24: test_highTouch() FAILED\n"); exit(1);
    }
    if (ht->counts != 0) {
      printf("F25: test_highTouch() FAILED\n"); exit(1);
    }
  }
  do_highTouch(bp->list[500]);
}


addNonSuppressedUsers(bucket *to, bucket *from, int type)
{
  int i;
  unsigned int *fl, *tl;
  int suppress;

  fl = from->list;
  tl = &(to->list[to->bsize]);
  for (i = 0; i < from->bsize; i++) {
if (*fl == 0) {
  printBucket(from);
}
    if (type == HT_ATTACK) {
      suppress = isUserSuppressAttack(*fl);
    }
    else {
      suppress = isUserSuppressAll(*fl);
    }
    if (suppress == 0) {
      // don't need to suppress, so add to bucket
      *tl = *fl;
      tl++;
      to->bsize++;
    }
    else {
//printf("-------------------%x is suppressed ----------------\n", (int)(*fl & 0xffff));
    }
    fl++;
  }
}
