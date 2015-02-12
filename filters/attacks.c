#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./filters.h"
#include "./utilities.h"
#include "./defense.h"

// externs needed to keep compiler from warning
extern bucket *makeBucket(int arg1);
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeRandomBucketFromList(int arg1, bucket *arg2);
extern bucket *dupBucket(bucket *arg1);
extern bucket *combineBuckets(bucket *arg1, bucket *arg2);
extern bucket *createHighTouchTable(int arg1);
extern bucket *makeSegregatedBucketFromList(int mask, 
                             int sampleShift,
                             int max_bsize, 
                             int sampleNum,
                             int childNum,
                             unsigned int victim,
                             bucket *userList, 
                             int userListSize);


#define USER_LIST_SIZE 0x100000  

// number of attack rounds: needed to be confident in my confidence level
#define NUM_ROUNDS 40	

float diffAttackDiffs[NUM_ROUNDS];
int attackRoundNum;
int rightGuesses;
int wrongGuesses;
int notSure;

int *accuracy;
int accIndex;
int accSize;

// This routine lets us avoid having too large, or too
// small bucket sizes (given that the number of needed
// segregate buckets can vary widely)
int
getSegregateMask(int numSamples, 
                 int numChildren, 
                 int *mask, 
                 int *max_bsize, 
                 int *shift)
{
  int bsize;

  // first compute shift value needed to shift numSamples past
  // numChildren
  *shift = 0;
  *mask = 1;
  while (*mask < (unsigned int) 0xffff) {
    if (*mask > numChildren) {
      break;
    }
    *mask = (*mask << 1) | 1;
    (*shift)++;
  }

  bsize = (int) ((float)USER_LIST_SIZE / 
                               ((float)numSamples * (float)numChildren));
  if (bsize > 200) {
    bsize = 200;  // don't really need buckets bigger than this for testing
  }
  else if (bsize < 32) {
    printf("getSegregateMask bsize (%d) too small (%d samples, %d children)\n",
                         bsize, numSamples, numChildren);
    exit(1);
  }
  // now pick a mask that roughly gives us this bucket size
  *mask = 0xff; 
  while (*mask < (unsigned int) 0xffffff) {
    if ((int)((float) USER_LIST_SIZE / (float) *mask) < bsize) {
      break;
    }
    *mask = (*mask << 1) | 1;
  }
  // finally, set a conservative value for max expected bucket size
  *max_bsize = bsize * 3;
}

initAttackStats(int numSamples, attack_setup *as)
{
  int i;

  attackRoundNum = 0;
  for (i = 0; i < NUM_ROUNDS; i++) {
    diffAttackDiffs[i] = 0;
  }
  if (as->numChildren == 0) {
    accSize = numSamples * NUM_ROUNDS * 2;
  }
  else {
    accSize = numSamples * NUM_ROUNDS * (as->numChildren + 1);
  }
  accuracy = (int *) calloc(accSize, sizeof (int *));
  accIndex = 0;
}

computeAttackStats(int numSamples, attack_setup *as) {
  mystats accS, diffS;

  getStatsInt(&accS, accuracy, accIndex);
  getStatsFloat(&diffS, diffAttackDiffs, attackRoundNum);
  printf("\n%d samples, confidence %d%%, error: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         numSamples,
         (int)(((float) rightGuesses / (float) NUM_ROUNDS) * 100.0),
         accS.av, accS.sd, accS.min, accS.max);
  printf("            answers: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         diffS.av, diffS.sd, diffS.min, diffS.max);
  
  free(accuracy);
}

makeChaffBuckets(bucket *userList, attack_setup *as) {
  int temp, bsize1, bsize2, i, chaffAmount, noisyCount;
  int overlap, absNumOverlap;
  bucket *bp1, *bp2;
  compare c;

  chaffAmount = getRandInteger(as->chaffMin, as->chaffMax);
  for (i = 0; i < chaffAmount; i += 2) {
    bsize1 = getRandInteger(25, 400);
    bsize2 = getRandInteger(25, 400);
    if (bsize1 > bsize2) {
      temp = bsize2;
      bsize2 = bsize1;
      bsize1 = temp;
    }
    bp1 = makeRandomBucketFromList(bsize1, userList);
    bp2 = makeRandomBucketFromList(bsize2, userList);
    if (bsize1 & 1) {
      overlap = getRandInteger(0,/* zzzz 100*/2);
      absNumOverlap = (int) ((float) (bsize1 * overlap) / (float) 100.0);
      if (absNumOverlap > bsize1) {
      // should never happen
        absNumOverlap = bsize1;
      }
      //printf("Attack absNumOverlap %d (of %d)\n", absNumOverlap, bsize1);
      makeCompareBucketFixed(bp1, bp2, absNumOverlap);
    }
    else {
      overlap = 0;   // only for reporting
    }
    // printf("Attack: size %d, overlap %d\n", bp1->bsize, overlap);
    noisyCount = putBucket(bp1, as);
    // printf("Attack: size %d, overlap %d\n", bp2->bsize, overlap);
    noisyCount = putBucket(bp2, as);
  }
}

float
oneMtOattack(int numSamples, bucket *userList, attack_setup *as)
{
  int i, j, mask, max_bsize, shift;
  int sum1=0, sum2=0;
  int noisyCount;
  float av1, av2;
  bucket *pbp, *vbp, *temp;  // *vbp = victim
  bucket **cbp;   // children

  cbp = (bucket **) calloc(as->numChildren, sizeof(bucket *));

  vbp = makeRandomBucketFromList(1, userList);
  for (i = 0; i < numSamples; i++){
    makeChaffBuckets(userList, as);
    pbp = makeBucket(0);
    for (j = 0; j < as->numChildren; j++) {
      // the mask and max_bsize here ensure that I get, on one hand,
      // somewhat large buckets, but on the other, not more than
      // the userList can handle
      getSegregateMask(numSamples, as->numChildren, &mask, &max_bsize, &shift);
      cbp[j] = makeSegregatedBucketFromList(mask, shift, max_bsize, i, j,
                         *(vbp->list), userList, USER_LIST_SIZE);
      temp = combineBuckets(cbp[j], pbp);
      freeBucket(pbp); pbp = temp;
    }
    // now I have parent and children, without victim nor yet put
    // add victim where it belongs
    if (as->attribute == VICTIM_ATTRIBUTE_YES) {
      if (as->location == VICTIM_IN_PARENT) {
        temp = combineBuckets(pbp, vbp);
        freeBucket(pbp); pbp = temp;
      }
      else if (as->location == VICTIM_IN_ONE_CHILD) {
        temp = combineBuckets(cbp[0], vbp);
        freeBucket(cbp[0]); cbp[0] = temp;
      }
      else if (as->location == VICTIM_IN_ALL_CHILDREN) {
        for (j = 0; j < as->numChildren; j++) {
          temp = combineBuckets(cbp[j], vbp);
          freeBucket(cbp[j]); cbp[j] = temp;
        }
      }
    }
    // now give all the buckets to the defense, in the right order
    if (accIndex >= accSize) {
      printf("oneMtOAttack() accIndex %d exceeds %d\n", accIndex, accSize);
      exit(1);
    }
    if (((as->location == VICTIM_IN_PARENT) && (as->order == VICTIM_FIRST)) ||
       ((as->location != VICTIM_IN_PARENT) && (as->order == VICTIM_LAST))) {
      noisyCount = putBucket(pbp, as);
      accuracy[accIndex++] = pbp->bsize - noisyCount;
      sum1 += noisyCount;
    }
    if (as->order == VICTIM_LAST) {
      for (j = (as->numChildren-1); j >= 0; j--){
        // go in reverse order here so that the victim bucket goes last
        noisyCount = putBucket(cbp[j], as);
        accuracy[accIndex++] = (cbp[j])->bsize - noisyCount;
        sum2 += noisyCount;
      }
    }
    else if (as->order == VICTIM_FIRST) {
      for (j = 0; j < as->numChildren; j++) {
        noisyCount = putBucket(cbp[j], as);
        accuracy[accIndex++] = (cbp[j])->bsize - noisyCount;
        sum2 += noisyCount;
      }
    }
    if (!(((as->location == VICTIM_IN_PARENT) && (as->order == VICTIM_FIRST)) ||
       ((as->location != VICTIM_IN_PARENT) && (as->order == VICTIM_LAST)))) {
      // didn't put parent before, so do it now
      noisyCount = putBucket(pbp, as);
      accuracy[accIndex++] = pbp->bsize - noisyCount;
      sum1 += noisyCount;
    }
  }

  freeBucket(vbp);
  av1 = (float)((float) sum1 / (float) numSamples);
  av2 = (float)((float) sum2 / (float) numSamples);
  return(av2 - av1);
}

float
oneOtOattack(int numSamples, bucket *userList, attack_setup *as)
{
  int i;
  int sum1=0, sum2=0;
  int noisyCount;
  float av1, av2;
  bucket *bp1, *bp2, *vbp, *temp;  // *vbp = victim

  // In this OtO diff attack, one set of buckets has the victim
  // if the victim has the attribute, and the other set of buckets never
  // has the victim.
  vbp = makeRandomBucketFromList(1, userList);
  //printf("********VICTIM is %x********\n", vbp->list[0]);
  makeChaffBuckets(userList, as);
  for (i = 0; i < numSamples; i++){
    // There is a chance the victim will be in this bucket too
    // Statistically this won't effect things much
    bp1 = makeRandomBucketFromList(getRandInteger(20, 200), userList);
    if (as->attribute == VICTIM_ATTRIBUTE_YES) {
      bp2 = combineBuckets(bp1, vbp);
    }
    // bp1 and bp2 are identical, except bp2 has victim
    if (as->order == VICTIM_FIRST) {
      // make bp1 hold the victim
      temp = bp1; bp1 = bp2; bp2 = temp;
    }
    noisyCount = putBucket(bp1, as);
    accuracy[accIndex++] = bp1->bsize - noisyCount;
    sum1 += noisyCount;
    makeChaffBuckets(userList, as);
    noisyCount = putBucket(bp2, as);
    accuracy[accIndex++] = bp2->bsize - noisyCount;
    sum2 += noisyCount;
    makeChaffBuckets(userList, as);
  }
  freeBucket(vbp);
  av1 = (float)((float) sum1 / (float) numSamples);
  av2 = (float)((float) sum2 / (float) numSamples);
  return(av2 - av1);
}

runAttack(bucket *userList, attack_setup *as)
{
  int confidence;   // between 0 and 100 percent
  int i, numSamples;
  float answer;
  
  for (numSamples = 5; numSamples < 81; numSamples *= 2) {
    initDefenseStats();
    initAttackStats(numSamples, as);
    rightGuesses = 0;
    notSure = 0;
    wrongGuesses = 0;
    for (i = 0; i < NUM_ROUNDS; i++) {
      initDefense(10000);
      if (as->attack == OtO_ATTACK) {
        answer = oneOtOattack(numSamples, userList, as);
        if (as->order == VICTIM_LAST) {
          if (answer >= 0.5) { rightGuesses++; }
          else { wrongGuesses++; }
        }
        else if (as->order == VICTIM_FIRST) {
          if (answer <= -0.5) { rightGuesses++; }
          else { wrongGuesses++; }
        }
      }
      else if (as->attack == MtO_ATTACK) {
        answer = oneMtOattack(numSamples, userList, as);
        if (as->location == VICTIM_IN_PARENT) {
          if (answer <= 0.5) { rightGuesses++; }
          else { wrongGuesses++; }
        }
        else { if (answer >= -0.5) { rightGuesses++;
          }
          else { wrongGuesses++; }
        }
      }
      endDefense();
      if (attackRoundNum == NUM_ROUNDS) {
        printf("runAttack(): bad attackRoundNum %d\n", attackRoundNum);
        exit(1);
      }
      diffAttackDiffs[attackRoundNum++] = answer;
    }
    computeAttackStats(numSamples, as);
    computeDefenseStats(NUM_ROUNDS, as);
  }
}

do_getSegregateMask(bucket *userList, int numSamples, int numChildren)
{
  int mask, max_bsize, shift;
  int i, j;
  int max=0, min=100000000;
  bucket *bp;

  getSegregateMask(numSamples, numChildren, &mask, &max_bsize, &shift);
  printf("\n%d samples, %d children, mask %x (%d), max_bsize %d, shift %d\n",
                     numSamples, numChildren, mask, mask, max_bsize, shift);
  printf("Expect %d users per bucket\n", 
         (int)((float) USER_LIST_SIZE / (float)mask));

  for (i = 0; i < numSamples; i++) {
    for (j = 0; j < numChildren; j++) {
      bp = makeSegregatedBucketFromList(mask, shift, max_bsize, i, j,
                             0x38447262, userList, USER_LIST_SIZE);
      max = (bp->bsize > max)?bp->bsize:max;
      min = (bp->bsize < min)?bp->bsize:min;
    }
  }
  printf("max bucket %d, min bucket %d\n", max, min);
}

test_getSegregateMask(bucket *userList)
{
  int numSamples, numChildren;

  numSamples = 10; numChildren = 2;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 20; numChildren = 2;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 80; numChildren = 16;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 160; numChildren = 20;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 320; numChildren = 20;
  do_getSegregateMask(userList, numSamples, numChildren);

  numSamples = 640; numChildren = 20;
  do_getSegregateMask(userList, numSamples, numChildren);
}

main()
{
  bucket *userList;
  attack_setup as;
  as.defense_str[BASIC_DEFENSE] = "basic defense";
  as.defense_str[OtO_DEFENSE] = "OtO defense"; 
  as.defense_str[MtO_DEFENSE] = "MtO defense"; 
  as.defense_str[MtM_DEFENSE] = "MtM defense";
  as.order_str[VICTIM_FIRST] = "victim first"; 
  as.order_str[VICTIM_LAST] = "victim last"; 
  as.order_str[VICTIM_RANDOM] = "victim random";
  as.attribute_str[VICTIM_ATTRIBUTE_YES] = "victim has attribute"; 
  as.attribute_str[VICTIM_ATTRIBUTE_NO] = "victim does not have attribute"; 
  as.attack_str[OtO_ATTACK] = "OtO attack"; 
  as.attack_str[MtO_ATTACK] = "MtO attack"; 
  as.attack_str[MtM_ATTACK] = "MtM attack";
  as.subAttack_str[RANDOM_CHILDREN] = "random children"; 
  as.subAttack_str[SEGREGATED_CHILDREN] = "segregated children"; 
  as.location_str[VICTIM_IN_PARENT] = "victim in parent";
  as.location_str[VICTIM_IN_ONE_CHILD] = "victim in one child";
  as.location_str[VICTIM_IN_ALL_CHILDREN] = "victim in all children";

  srand48((long int) 2);

  // Make complete list of users used for all attacks
  userList = createHighTouchTable(USER_LIST_SIZE);

  as.attack = MtO_ATTACK;
  //as.attack = OtO_ATTACK;

  as.defense = BASIC_DEFENSE;
  //as.defense = OtO_DEFENSE;
  //as.defense = MtO_DEFENSE;

  as.order = VICTIM_LAST;
  //as.order = VICTIM_FIRST;

  //as.location = VICTIM_IN_PARENT;
  //as.location = VICTIM_IN_ALL_CHILDREN;
  as.location = VICTIM_IN_ONE_CHILD;

  as.attribute = VICTIM_ATTRIBUTE_YES;
  //as.attribute = VICTIM_ATTRIBUTE_NO;

  as.subAttack = SEGREGATED_CHILDREN;

  as.numChildren = 2;

  as.chaffMin = 0;
  as.chaffMax = 0;
  printf("Attack: %s, %s, %s, %s, %s\n", as.attack_str[as.attack],
                                     as.defense_str[as.defense], 
                                     as.order_str[as.order],
                                     as.location_str[as.location],
                                     as.attribute_str[as.attribute]);
  //test_getSegregateMask(userList); exit(1);
  runAttack(userList, &as);
}
