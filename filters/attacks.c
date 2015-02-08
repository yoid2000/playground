#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "./filters.h"
#include "./utilities.h"
#include "./defense.h"

// externs needed to keep compiler from warning
extern bucket *makeRandomBucket(int arg1);
extern bucket *makeRandomBucketFromList(int arg1, bucket *arg2);
extern bucket *dupBucket(bucket *arg1);
extern bucket *combineBuckets(bucket *arg1, bucket *arg2);
extern bucket *createHighTouchTable(int arg1);

#define USER_LIST_SIZE 16384    // gives 256 groups of 64 users, for instance

// number of attack rounds: needed to be confident in my confidence level
#define NUM_ROUNDS 40	

float diffAttackDiffs[NUM_ROUNDS];
int attackRoundNum;
int rightGuesses;
int wrongGuesses;
int notSure;

int *accuracy;
int accIndex;

initAttackStats(int numSamples)
{
  int i, accSize;

  attackRoundNum = 0;
  for (i = 0; i < NUM_ROUNDS; i++) {
    diffAttackDiffs[i] = 0;
  }
  accSize = numSamples * NUM_ROUNDS * 2;
  accuracy = (int *) calloc(accSize, sizeof (int *));
  accIndex = 0;
}

computeAttackStats(numSamples) {
  int accSize;
  mystats accS, diffS;

  accSize = numSamples * NUM_ROUNDS * 2;
  if (accIndex != accSize) {
    printf("Something wrong with accIndex %d (should be %d)\n", 
            accIndex, accSize);
    exit(1);
  }
  getStatsInt(&accS, accuracy, accIndex);
  getStatsFloat(&diffS, diffAttackDiffs, attackRoundNum);
  printf("%d samples, confidence %d%%, error: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         numSamples,
         (int)(((float) rightGuesses / (float) NUM_ROUNDS) * 100.0),
         accS.av, accS.sd, accS.min, accS.max);
  printf("            answers: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         diffS.av, diffS.sd, diffS.min, diffS.max);
  
  free(accuracy);
}

makeChaffBuckets(bucket *userList, int low, int high, attack_setup *as) {
  int temp, bsize1, bsize2, i, chaffAmount, noisyCount;
  int overlap, absNumOverlap;
  bucket *bp1, *bp2;
  compare c;

  chaffAmount = getRandInteger(low, high);
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
oneOtOattack(int numSamples, bucket *userList, attack_setup *as)
{
  int i;
  int sum1=0, sum2=0;
  int noisyCount;
  float av1, av2;
  bucket *bp1, *bp2, *vbp, *temp;  // *vbp = victim

  // In the naive diff attack, one set of buckets will have the victim
  // if the victim has the attribute, and the other set of buckets will
  // have the victim otherwise.  Therefore for the sake of this simulation,
  // it doesn't matter which set we put the victim in.
  vbp = makeRandomBucketFromList(1, userList);
  //printf("********VICTIM is %x********\n", vbp->list[0]);
  makeChaffBuckets(userList, 25, 75, as);
  for (i = 0; i < numSamples; i++){
    // There is a chance the victim will be in this bucket too
    // Statistically this won't effect things much
    bp1 = makeRandomBucketFromList(getRandInteger(20, 200), userList);
    bp2 = combineBuckets(bp1, vbp);
    // bp1 and bp2 are identical, except bp2 has victim
    if (as->order == VICTIM_FIRST) {
      // make bp1 hold the victim
      temp = bp1; bp1 = bp2; bp2 = temp;
    }
    noisyCount = putBucket(bp1, as);
    accuracy[accIndex++] = bp1->bsize - noisyCount;
    sum1 += noisyCount;
    makeChaffBuckets(userList, 15, 30, as);
    noisyCount = putBucket(bp2, as);
    accuracy[accIndex++] = bp2->bsize - noisyCount;
    sum2 += noisyCount;
    makeChaffBuckets(userList, 15, 30, as);
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
    initAttackStats(numSamples);
    rightGuesses = 0;
    notSure = 0;
    for (i = 0; i < NUM_ROUNDS; i++) {
      initDefense(10000);
      if (as->attack == OtO_ATTACK) {
        answer = oneOtOattack(numSamples, userList, as);
      }
      endDefense();
      if (as->order == VICTIM_LAST) {
        if (answer > 0.5) {
          rightGuesses++;
        }
      }
      else if (as->order == VICTIM_FIRST) {
        if (answer < -0.5) {
          rightGuesses++;
        }
      }
      if (attackRoundNum == NUM_ROUNDS) {
        printf("runAttack(): bad attackRoundNum %d\n", attackRoundNum);
        exit(1);
      }
      diffAttackDiffs[attackRoundNum++] = answer;
    }
    computeAttackStats(numSamples);
    computeDefenseStats(NUM_ROUNDS, as);
  }
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
  as.attribute_str[VICTIM_ATTRIBUTE_RANDOM] = "victim attribute random";
  as.attack_str[OtO_ATTACK] = "OtO attack"; 
  as.attack_str[MtO_ATTACK] = "MtO attack"; 
  as.attack_str[MtM_ATTACK] = "MtM attack";

  srand48((long int) 2);

  // Make complete list of users used for all attacks
  userList = createHighTouchTable(USER_LIST_SIZE);

  as.attack = OtO_ATTACK;
  //as.defense = BASIC_DEFENSE;
  as.defense = OtO_DEFENSE;
  as.order = VICTIM_LAST;
  //as.order = VICTIM_FIRST;
  as.attribute = VICTIM_ATTRIBUTE_YES;
  printf("Attack: %s, %s, %s, %s\n", as.attack_str[as.attack],
                                     as.defense_str[as.defense], 
                                     as.order_str[as.order],
                                     as.attribute_str[as.attribute]);
  runAttack(userList, &as);
}
