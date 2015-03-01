#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fenv.h>
#include "./filters.h"
#include "./utilities.h"
#include "./attacks.h"

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
extern blocks *defineBlocks(int samples, int blocksPerSample, int *lastBlock);
extern float putBucket(bucket *bp, attack_setup *as);

#define USER_LIST_SIZE 0x100000  

float *diffAttackDiffs;
int attackRoundNum;
int rightGuesses;
int wrongGuesses;
int notSure;

float *accuracy;
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
  *shift = 1;
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
  for (i = 0; i < as->numRounds; i++) {
    diffAttackDiffs[i] = 0;
  }
  if (as->attack == OtO_ATTACK) {
    accSize = numSamples * as->numRounds * 2;
  }
  else if (as->attack == MtO_ATTACK) {
    accSize = numSamples * as->numRounds * (as->numChildren + 1);
  }
  else if (as->attack == MtM_ATTACK) {
    accSize = numSamples * as->numRounds * 
                                (as->maxLeftBuckets + as->maxRightBuckets);
  }
  accuracy = (float *) calloc(accSize, sizeof (float *));
  accIndex = 0;
}

computeAttackStats(int numSamples, attack_setup *as) {
  mystats accS, diffS;

  getStatsFloat(&accS, accuracy, accIndex);
  getStatsFloat(&diffS, diffAttackDiffs, attackRoundNum);
  fprintf(as->f, "\n%d samples, right %d%%, answers: av %.2f, sd = %.2f, error: av = %.2f, sd = %.2f\n",
         numSamples,
         (int)(((float) rightGuesses / (float) as->numRounds) * 100.0),
         diffS.av, diffS.sd, accS.av, accS.sd);
  fprintf(as->f, "            answers: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         diffS.av, diffS.sd, diffS.min, diffS.max);
  fprintf(as->f, "            error: av %.2f, sd = %.2f, min = %.2f, max = %.2f\n",
         accS.av, accS.sd, accS.min, accS.max);
  
  free(accuracy);
}

makeChaffBuckets(bucket *userList, attack_setup *as) {
  int temp, bsize1, bsize2, i, chaffAmount;
  float noisyCount;
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

int
worstCaseBlocks(attack_setup *as)
{
  return(as->numBaseBlocks + as->numExtraBlocks +
                        as->maxLeftBuckets + as->maxRightBuckets - 1);
}

float
oneMtMattack(int numSamples, 
             bucket *userList, 
             attack_setup *as)
{
  int i, j, blockIndex=0;
  int baseBlocks;
  float noisyCount;
  int mask, max_bsize, shift;
  bucket *temp, *vbp;  // victim
  mtm_cluster mc;
  bucket **bbp;   // temporary buckets for each block
  bucket *bp[2][MAX_NUM_BUCKETS_PER_SIDE];    // actual buckets
  int s, b;     // side and bucket for bp[][]
  blocks *block_array = NULL;
  int block, lastBlock, first_s;
  float sums[2];
  float av1, av2;

  sums[LEFT] = 0; sums[RIGHT] = 0;

  if ((as->maxLeftBuckets > MAX_NUM_BUCKETS_PER_SIDE) ||
      (as->maxRightBuckets > MAX_NUM_BUCKETS_PER_SIDE)) {
    printf("oneMtMattack ERROR1 %d, %d\n",  
                   as->maxLeftBuckets, as->maxRightBuckets);
    exit(1);
  }

  block_array = defineBlocks(numSamples, worstCaseBlocks(as), &lastBlock);
  bbp = (bucket **) calloc(worstCaseBlocks(as), sizeof(bucket *));

  // make victim
  vbp = makeRandomBucketFromList(1, userList);
  getSegregateMask(numSamples, worstCaseBlocks(as),
                                           &mask, &max_bsize, &shift);
  for (i = 0; i < numSamples; i++) {
    makeChaffBuckets(userList, as);
    as->numLeftBuckets = getRandInteger(as->minLeftBuckets, 
                                                  as->maxLeftBuckets);
    as->numRightBuckets = getRandInteger(as->minRightBuckets, 
                                                  as->maxRightBuckets);
    baseBlocks = as->numBaseBlocks + 
                 as->numLeftBuckets + as->numRightBuckets - 1;

    if ((blockIndex + baseBlocks) > lastBlock) {
      printf("oneMtMattack ERROR2 %d, %d, %d, %d\n", i, blockIndex, baseBlocks,
                                                 lastBlock);
      printAttackSetup(as);
      exit(1);
    }

    if ((baseBlocks + as->numExtraBlocks) > MAX_NUM_BLOCKS) {
      printf("oneMtMattack ERROR3 %d, %d\n",  
                     baseBlocks, as->numExtraBlocks);
      exit(1);
    }

    if (defineCluster(&mc, baseBlocks, as) == 0) {
      printf("oneMtMattack ERROR4 %d\n", baseBlocks);
      printAttackSetup(as);
      printMtmCluster(&mc);
      exit(1);
    }
    // build all the per-block user lists
    for (j = 0; j < worstCaseBlocks(as); j++) {
      bbp[j] = makeSegregatedBucketFromList(mask, shift, max_bsize, 
                         block_array[blockIndex].sampleNum, 
                         block_array[blockIndex].childNum, 
                         *(vbp->list), userList, USER_LIST_SIZE);
      blockIndex++;
    }

    // build the buckets from the per-block user lists
    for (s = 0; s < 2; s++) {
      for (b = 0; b < mc.numBuckets[s]; b++) {
        mc.bucket[s][b].bp = makeBucket(0);
        for (j = 0; j < mc.bucket[s][b].numBlocks; j++) {
          block = mc.bucket[s][b].blocks[j];
          temp = combineBuckets(bbp[block], mc.bucket[s][b].bp);
          freeBucket(mc.bucket[s][b].bp); mc.bucket[s][b].bp = temp;
        }
      }
    }

    // free all of the per-block user lists
    for (j = 0; j < worstCaseBlocks(as); j++) {
      freeBucket(bbp[j]);
    }

    // ok, now we have a user-list bucket for each bucket in the
    // cluster.  We need to add the victim in the right place(s),
    // and set whether right or left will be submitted first
    if (as->attribute == VICTIM_ATTRIBUTE_YES) {
      if (as->location == VICTIM_IN_ONE_RIGHT) {
        if (as->order == VICTIM_FIRST) {
          first_s = RIGHT;
          b = 0;
        }
        else {
          first_s = LEFT;
          b = mc.numBuckets[RIGHT] - 1;
        }
        temp = combineBuckets(mc.bucket[RIGHT][b].bp, vbp);
        freeBucket(mc.bucket[RIGHT][b].bp); mc.bucket[RIGHT][b].bp = temp;
      }
      else {
        first_s = RIGHT;
        if (as->location == VICTIM_IN_ALL_RIGHT) { 
          s = RIGHT; 
          first_s = RIGHT;    // assumes order = VICTIM_FIRST
        }
        else if (as->location == VICTIM_IN_ALL_LEFT) { 
          s = LEFT; 
          first_s = LEFT;    // assumes order = VICTIM_FIRST
        }
        if (as->order == VICTIM_LAST) {
          // assumption wrong, so swap sides
          first_s = (first_s==RIGHT)?LEFT:RIGHT;
        }
        for (b = 0; b < mc.numBuckets[s]; b++) {
          temp = combineBuckets(mc.bucket[s][b].bp, vbp);
          freeBucket(mc.bucket[s][b].bp); mc.bucket[s][b].bp = temp;
        }
      }
    }
    // and finally feed them to the defense in the appropriate order.
    if (accIndex >= accSize) {
      printf("oneMtMAttack() accIndex %d exceeds %d\n", accIndex, accSize);
      exit(1);
    }
    s = first_s;
    for (b = 0; b < mc.numBuckets[s]; b++) {
      noisyCount = putBucket(mc.bucket[s][b].bp, as);
      accuracy[accIndex++] = (float)((mc.bucket[s][b].bp)->bsize) - noisyCount;
      sums[s] += noisyCount;
    }
    s = (s==RIGHT)?LEFT:RIGHT;
    for (b = 0; b < mc.numBuckets[s]; b++) {
      noisyCount = putBucket(mc.bucket[s][b].bp, as);
      accuracy[accIndex++] = (float)((mc.bucket[s][b].bp)->bsize) - noisyCount;
      sums[s] += noisyCount;
    }
  }
  // report
  free(bbp);
  free(block_array);
  freeBucket(vbp);
  av1 = (float)(sums[LEFT] / (float) numSamples);
  av2 = (float)(sums[RIGHT] / (float) numSamples);
  return(av2 - av1);
}

float
oneMtOattack(int numSamples, bucket *userList, attack_setup *as)
{
  int i, j, mask, max_bsize, shift;
  float sum1=0.0, sum2=0.0;
  float noisyCount;
  float av1, av2;
  bucket *pbp, *vbp, *temp;  // *vbp = victim
  bucket **cbp;   // children

  cbp = (bucket **) calloc(as->numChildren, sizeof(bucket *));

  vbp = makeRandomBucketFromList(1, userList);
  // the mask and max_bsize here ensure that I get, on one hand,
  // somewhat large buckets, but on the other, not more than
  // the userList can handle
  getSegregateMask(numSamples, as->numChildren, &mask, &max_bsize, &shift);
  for (i = 0; i < numSamples; i++){
    makeChaffBuckets(userList, as);
    pbp = makeBucket(0);
    for (j = 0; j < as->numChildren; j++) {
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
      accuracy[accIndex++] = (float) pbp->bsize - noisyCount;
      sum1 += noisyCount;
    }
    if (as->order == VICTIM_LAST) {
      for (j = (as->numChildren-1); j >= 0; j--){
        // go in reverse order here so that the victim bucket goes last
        noisyCount = putBucket(cbp[j], as);
        accuracy[accIndex++] = (float) (cbp[j])->bsize - noisyCount;
        sum2 += noisyCount;
      }
    }
    else if (as->order == VICTIM_FIRST) {
      for (j = 0; j < as->numChildren; j++) {
        noisyCount = putBucket(cbp[j], as);
        accuracy[accIndex++] = (float) (cbp[j])->bsize - noisyCount;
        sum2 += noisyCount;
      }
    }
    if (!(((as->location == VICTIM_IN_PARENT) && (as->order == VICTIM_FIRST)) ||
       ((as->location != VICTIM_IN_PARENT) && (as->order == VICTIM_LAST)))) {
      // didn't put parent before, so do it now
      noisyCount = putBucket(pbp, as);
      accuracy[accIndex++] = (float) pbp->bsize - noisyCount;
      sum1 += noisyCount;
    }
  }

  free(cbp);
  freeBucket(vbp);
  av1 = (float)(sum1 / (float) numSamples);
  av2 = (float)(sum2 / (float) numSamples);
  return(av2 - av1);
}

float
oneOtOattack(int numSamples, bucket *userList, attack_setup *as)
{
  int i;
  float sum1=0.0, sum2=0.0;
  float noisyCount;
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
    else if (as->attribute == VICTIM_ATTRIBUTE_NO) {
      bp2 = dupBucket(bp1);
    }
    // bp1 and bp2 are identical, except bp2 has victim
    if (as->order == VICTIM_FIRST) {
      // make bp1 hold the victim
      temp = bp1; bp1 = bp2; bp2 = temp;
    }
    noisyCount = putBucket(bp1, as);
    accuracy[accIndex++] = (float) bp1->bsize - noisyCount;
    sum1 += noisyCount;
    makeChaffBuckets(userList, as);
    noisyCount = putBucket(bp2, as);
    accuracy[accIndex++] = (float) bp2->bsize - noisyCount;
    sum2 += noisyCount;
    makeChaffBuckets(userList, as);
  }
  freeBucket(vbp);
  av1 = (float)(sum1 / (float) numSamples);
  av2 = (float)(sum2 / (float) numSamples);
  return(av2 - av1);
}

makeDecision(float answer, attack_setup *as)
{
  switch(as->attack) {
    case OtO_ATTACK:
      switch(as->attribute) {
        case VICTIM_ATTRIBUTE_YES:
          switch(as->order) {
            case VICTIM_LAST:
              if (answer >= 0.5) { rightGuesses++; }
              else { wrongGuesses++; }
              break;
            case VICTIM_FIRST:
              if (answer <= -0.5) { rightGuesses++; }
              else { wrongGuesses++; }
              break;
            default:
              printf("makeDecision() should not get here 4\n"); exit(1);
          }
          break;
        case VICTIM_ATTRIBUTE_NO:
          if ((answer < 0.5) && (answer > -0.5)) { rightGuesses++; }
          else { wrongGuesses++; }
          break;
        default:
          printf("makeDecision() should not get here 2\n"); exit(1);
      }
      break;
    case MtO_ATTACK:
      switch(as->attribute) {
        case VICTIM_ATTRIBUTE_YES:
          switch(as->location) {
            case VICTIM_IN_PARENT:
              if (answer <= -0.5) { rightGuesses++; }
              else { wrongGuesses++; }
              break;
            case VICTIM_IN_ALL_CHILDREN:
            case VICTIM_IN_ONE_CHILD:
              if (answer >= 0.5) { rightGuesses++; }
              else { wrongGuesses++; }
              break;
            default:
              printf("makeDecision() should not get here 6\n"); exit(1);
          }
          break;
        case VICTIM_ATTRIBUTE_NO:
          if ((answer < 0.5) && (answer > -0.5)) { rightGuesses++; }
          else { wrongGuesses++; }
          break;
        default:
          printf("makeDecision() should not get here 3\n"); exit(1);
      }
      break;
    case MtM_ATTACK:
      switch(as->attribute) {
        case VICTIM_ATTRIBUTE_YES:
          switch(as->location) {
            case VICTIM_IN_ALL_LEFT:
              if (answer <= -0.5) { rightGuesses++; }
              else { wrongGuesses++; }
              break;
            case VICTIM_IN_ALL_RIGHT:
            case VICTIM_IN_ONE_RIGHT:
              if (answer >= 0.5) { rightGuesses++; }
              else { wrongGuesses++; }
              break;
            default:
              printf("makeDecision() should not get here 6\n"); exit(1);
          }
          break;
        case VICTIM_ATTRIBUTE_NO:
          if ((answer < 0.5) && (answer > -0.5)) { rightGuesses++; }
          else { wrongGuesses++; }
          break;
        default:
          printf("makeDecision() should not get here 3\n"); exit(1);
      }
      break;
    default:
      printf("makeDecision() should not get here 1\n"); exit(1);
  }
}

runAttack(bucket *userList, attack_setup *as)
{
  int confidence;   // between 0 and 100 percent
  int i, numSamples;
  float answer;
  
  initDefenseStats();
  initAttackStats(as->numSamples, as);
  rightGuesses = 0;
  notSure = 0;
  wrongGuesses = 0;
  for (i = 0; i < as->numRounds; i++) {
    initDefense(10000);
    if (as->attack == OtO_ATTACK) {
      answer = oneOtOattack(as->numSamples, userList, as);
      makeDecision(answer, as);
    }
    else if (as->attack == MtO_ATTACK) {
      answer = oneMtOattack(as->numSamples, userList, as);
      makeDecision(answer, as);
    }
    else if (as->attack == MtM_ATTACK) {
      answer = oneMtMattack(as->numSamples, userList, as);
      makeDecision(answer, as);
    }
    endDefense();
    if (attackRoundNum == as->numRounds) {
      printf("runAttack(): bad attackRoundNum %d\n", attackRoundNum);
      exit(1);
    }
    diffAttackDiffs[attackRoundNum++] = answer;
  }
  computeAttackStats(as->numSamples, as);
  computeDefenseStats(as->numRounds, as);
}

printAttackSetup(attack_setup *as)
{

  fprintf(as->f, "Attack: %s, %s, %s, %s, %s\n", as->attack_str[as->attack],
                                     as->defense_str[as->defense], 
                                     as->order_str[as->order],
                                     as->location_str[as->location],
                                     as->attribute_str[as->attribute]);
  fprintf(as->f, " chaffMax %d\n", as->chaffMax);
  fprintf(as->f, " chaffMin %d\n", as->chaffMin);
  fprintf(as->f, " numRounds %d\n", as->numRounds);
  fprintf(as->f, " numSamples %d\n", as->numSamples);

  if (as->attack == MtM_ATTACK) {
    fprintf(as->f, " numBaseBlocks %d\n", as->numBaseBlocks);
    fprintf(as->f, " numExtraBlocks %d\n", as->numExtraBlocks);
    fprintf(as->f, " minLeftBuckets %d\n", as->minLeftBuckets);
    fprintf(as->f, " maxLeftBuckets %d\n", as->maxLeftBuckets);
    fprintf(as->f, " minRightBuckets %d\n", as->minRightBuckets);
    fprintf(as->f, " maxRightBuckets %d\n", as->maxRightBuckets);
    fprintf(as->f, " numLeftBuckets %d\n", as->numLeftBuckets);
    fprintf(as->f, " numRightBuckets %d\n", as->numRightBuckets);

  }
  if (as->attack == MtO_ATTACK) {
    fprintf(as->f, " numChildren %d\n", as->numChildren);
  }
}

printCommandLines(attack_setup *as)
{
  int i;

  printf("Usage: ./runAttacks -a attack -d defense -o victim_order -l victim_location -t victim_attribute -c num_children -m min_chaff -x max_chaff -r num_rounds -s num_samples -e seed -B num_base_blocks_past_min -E num_extra_blocks -W min_left -X max_left -Y min_right -Z max_right <directory>\n ");
  printf("     attack values:\n");
  for (i = 0; i < NUM_ATTACKS; i++) {
    printf("          %d = %s\n", i, as->attack_str[i]);
  }
  printf("     defense values:\n");
  for (i = 0; i < NUM_DEFENSES; i++) {
    printf("          %d = %s\n", i, as->defense_str[i]);
  }
  printf("     victim_order values:\n");
  for (i = 0; i < NUM_ORDERS; i++) {
    printf("          %d = %s\n", i, as->order_str[i]);
  }
  printf("     victim_location values:\n");
  for (i = 0; i < NUM_VICTIM_LOC; i++) {
    printf("          %d = %s\n", i, as->location_str[i]);
  }
  printf("     victim_attribute values:\n");
  for (i = 0; i < NUM_ATTRIBUTES; i++) {
    printf("          %d = %s\n", i, as->attribute_str[i]);
  }
  printf("     num_children between 2 and 20\n");
  printf("     min_chaff and max_chaff >= 0\n");
  printf("     num_rounds (for experiment statistical significance) > 0\n");
  printf("     num_samples (for attack statistical significance) > 0\n");
  printf("     seed:  any integer\n");
  
  printf("     num_base_blocks_past_min >= 0\n");
  printf("     num_extra_blocks >= 0\n");
  printf("     min_left >= 1\n");
  printf("     max_left >= min_left\n");
  printf("     min_right >= 1\n");
  printf("     max_right >= min_right\n");
}

main(int argc, char *argv[])
{
  bucket *userList;
  attack_setup as;
  int ran, c, seed=0;
  unsigned char path[378], dir[128], filename[256], temp[32];

  filename[0] = '\0';

  as.defense_str[BASIC_DEFENSE] = "basic defense";
  as.defense_str[OtO_DEFENSE] = "OtO defense"; 
  as.defense_str[MtO_DEFENSE] = "MtO defense"; 
  as.defense_str[MtM_DEFENSE] = "MtM defense";
  as.order_str[VICTIM_FIRST] = "victim first"; 
  as.order_str[VICTIM_LAST] = "victim last"; 
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
  as.location_str[VICTIM_IN_ALL_LEFT] = "victim in all left";
  as.location_str[VICTIM_IN_ALL_RIGHT] = "victim in all right";
  as.location_str[VICTIM_IN_ONE_RIGHT] = "victim in one right";

  // set defaults
  as.attack = MtO_ATTACK;
  as.defense = MtO_DEFENSE;
  as.order = VICTIM_LAST;
  as.location = VICTIM_IN_PARENT;
  as.attribute = VICTIM_ATTRIBUTE_YES;
  as.subAttack = SEGREGATED_CHILDREN;
  as.numChildren = 2;
  as.chaffMin = 0;
  as.chaffMax = 0;
  as.numRounds = 20;
  as.numSamples = 10;
  as.numBaseBlocks = 0;
  as.numExtraBlocks = 0;   
  as.minLeftBuckets = 2;   
  as.maxLeftBuckets = 2;   
  as.minRightBuckets = 2;   
  as.maxRightBuckets = 2;    
  as.numLeftBuckets = 0;     // gets set later
  as.numRightBuckets = 0;     // gets set later

  while ((c = getopt (argc, argv, "W:X:Y:Z:E:B:a:d:o:l:t:c:m:x:r:s:e:h?")) != -1) {
    sprintf(temp, "%c%s", c, optarg);
    strcat(filename, temp);
    switch (c) {
      case 'W':
        as.minLeftBuckets = atoi(optarg);
        break;
      case 'X':
        as.maxLeftBuckets = atoi(optarg);
        break;
      case 'Y':
        as.minRightBuckets = atoi(optarg);
        break;
      case 'Z':
        as.maxRightBuckets = atoi(optarg);
        break;
      case 'E':
        as.numExtraBlocks = atoi(optarg);
        break;
      case 'B':
        as.numBaseBlocks = atoi(optarg);
        break;
      case 'a':
        as.attack = atoi(optarg);
        break;
      case 'd':
        as.defense = atoi(optarg);
        break;
      case 'o':
        as.order = atoi(optarg);
        break;
      case 'l':
        as.location = atoi(optarg);
        break;
      case 't':
        as.attribute = atoi(optarg);
        break;
      case 'c':
        as.numChildren = atoi(optarg);
        break;
      case 'm':
        as.chaffMin = atoi(optarg);
        break;
      case 'x':
        as.chaffMax = atoi(optarg);
        break;
      case 'r':
        as.numRounds = atoi(optarg);
        break;
      case 's':
        as.numSamples = atoi(optarg);
        break;
      case 'e':
        seed = atoi(optarg);
        break;
      case '?':
      case 'h':
        printCommandLines(&as);
        exit(1);
      default:
        printCommandLines(&as);
        exit(1);
    }
  }
  if (seed == 0) {
    seed = time(NULL);
  }
  srand48((long int) seed);

  ran = quick_hash(filename) & 0xfff;
  sprintf(temp, ".%d", ran);
  strcat(filename, temp);
  strcat(filename, ".txt");

  if (optind == argc) {
    strcpy(dir, "./");
  }
  else {
    strcpy(dir, argv[optind]);
  }

  sprintf(path, "%s%s", dir, filename);
  if ((as.f = fopen(path, "w")) == NULL) {
    printf("Couldn't open file %s\n", path);
    exit(1);
  }

  fesetround(1);   // sets rounding to nearest integer

  // Make complete list of users used for all attacks
  userList = createHighTouchTable(USER_LIST_SIZE);
  diffAttackDiffs = (float *) calloc(as.numRounds, sizeof(float *));

  printAttackSetup(&as);

  //test_getSegregateMask(userList); exit(1);
  runAttack(userList, &as);
  printf("Done: %s\n", filename);
}

/************  TESTS ****************/

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
