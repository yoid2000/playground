#!/bin/bash

DIR=$(pwd)
#OUTDIR="/home/francis/attacks/"
OUTDIR="/root/paul/attacks/"
#OUTDIR="./"

D_NONE=0
D_BASIC=1
D_OTO=2
D_MTO=3
D_MTM=4
VFIRST=0
VLAST=1
ALL_CHILD=2
ALL_LEFT=3
ALL_RIGHT=4
ONE_RIGHT=5
ATT_YES=0
ATT_NO=1
ROUNDS="-a"
A_GEN=0
A_PER=1
A_BAR=2
A_NUN=3
A_NUNBAR=4

for DEFENSE in $D_MTM $D_MTO $D_BASIC $D_NONE
do
for ORDER in $VFIRST $VLAST
do
for USERS in 100 200 400 800 1600
do
for ATTACK in $A_GEN
do
for BASE in 0 4 8
do
for SIDE in $ALL_RIGHT $ONE_RIGHT
do
for RIGHT in 2 3 5 8 16
do
for LEFT in 2 3 5 8 16
do
for SAMP in 10 20 40 80 160
do
  echo "./newAttacks -c $ATTACK -l $LEFT -r $RIGHT -d $DEFENSE -o $ORDER -v $SIDE -m 0 -x 0 $ROUNDS 100 -s $SAMP -u $USERS -B $BASE -L 0 -R 0 -e 1 $OUTDIR"
  echo "./oldAttacks -c $ATTACK -l $LEFT -r $RIGHT -d $DEFENSE -o $ORDER -v $SIDE -m 0 -x 0 $ROUNDS 100 -s $SAMP -u $USERS -B $BASE -L 0 -R 0 -e 1 $OUTDIR"
done
done
done
done
done
done
done
done
done

for DEFENSE in $D_MTM $D_MTO $D_BASIC $D_NONE
do
for ORDER in $VFIRST $VLAST
do
for USERS in 100 200 400 800 1600
do
for ATTACK in $A_PER $A_BAR
do
for SIDE in $ALL_RIGHT $ONE_RIGHT
do
for LEFT in 2 3 5 8
do
for SAMP in 10 20 40 80 160
do
  echo "./newAttacks -c $ATTACK -l $LEFT -r $RIGHT -d $DEFENSE -o $ORDER -v $SIDE -m 0 -x 0 $ROUNDS 100 -s $SAMP -u $USERS -B $BASE -L 0 -R 0 -e 1 $OUTDIR"
  echo "./oldAttacks -c $ATTACK -l $LEFT -r $RIGHT -d $DEFENSE -o $ORDER -v $SIDE -m 0 -x 0 $ROUNDS 100 -s $SAMP -u $USERS -B $BASE -L 0 -R 0 -e 1 $OUTDIR"
done
done
done
done
done
done
done

for DEFENSE in $D_MTM $D_MTO $D_BASIC $D_NONE
do
for ORDER in $VFIRST $VLAST
do
for USERS in 100 200 400 800 1600
do
for ATTACK in $A_NUN $A_NUNBAR
do
for SIDE in $ALL_RIGHT $ONE_RIGHT
do
for SAMP in 10 20 40 80 160
do
  echo "./newAttacks -c $ATTACK -l $LEFT -r $RIGHT -d $DEFENSE -o $ORDER -v $SIDE -m 0 -x 0 $ROUNDS 100 -s $SAMP -u $USERS -B $BASE -L 0 -R 0 -e 1 $OUTDIR"
  echo "./oldAttacks -c $ATTACK -l $LEFT -r $RIGHT -d $DEFENSE -o $ORDER -v $SIDE -m 0 -x 0 $ROUNDS 100 -s $SAMP -u $USERS -B $BASE -L 0 -R 0 -e 1 $OUTDIR"
done
done
done
done
done
done

#Usage: ./newAttacks -c $ATTACK -l min_left -r min_right -L max_left -R max_right -d defense -o victim_order -v victim_location -t victim_attribute -m min_chaff -x max_chaff -a num_rounds -s num_samples -e seed -B num_base_blocks_past_min -u user_per_bucket <directory>
#      defense values:
#          0 = basic defense
#          1 = OtO defense
#          2 = MtO defense
#          3 = MtM defense
#     victim_order values:
#          0 = victim first
#          1 = victim last
#     victim_location values:
#          0 = victim in parent
#          1 = victim in one child
#          2 = victim in all children
#          3 = victim in all left
#          4 = victim in all right
#          5 = victim in one right
#     victim_attribute values:
#          0 = victim has attribute
#          1 = victim does not have attribute
#     min_chaff and max_chaff >= 0
#     num_rounds (for experiment statistical significance) > 0
#     num_samples (for attack statistical significance) > 0
#     seed:  any integer
#     num_base_blocks_past_min >= 0
#     users_per_bucket > 0
#     min_left >= 1
#     max_left >= min_left (0 means same as min)
#     min_right >= 1
#     max_right >= min_right (0 means same as min)
