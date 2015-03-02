#!/bin/bash

DIR=$(pwd)
OUTDIR="/home/francis/attacks/"

D_OTO=1
D_MTO=2
VFIRST=0
ALL_CHILD=2
ALL_LEFT=3
ALL_RIGHT=4
ATT_YES=0
ATT_NO=1
ROUNDS="-a"

for SIDE in $ALL_RIGHT # $ALL_LEFT
do
for RIGHT in 2
do
for LEFT in 1
do
for VIC in $ATT_YES # $ATT_NO
do
for SAMP in 40 # 80
do
 ./runAttacks -l $LEFT -r $RIGHT -d $D_MTO -o $VFIRST -v $SIDE -m 0 -x 0 $ROUNDS 80 -s $SAMP -t $VIC -B 0 -E 0 -L 0 -R 0 -e 1 $OUTDIR 
done
done
done
done
done
exit

for s in 5 10 20 40
do
  for t in 0 1
  do
#./runAttacks -a 0 -d 3 -o 0 -l 0 -c 2 -m 0 -x 0 -r 500 -s $s -t $t $OUTDIR
    for c in 10 20
    do
./runAttacks -a 1 -d 3 -o 0 -l 0 -c $c -m 0 -x 0 -r 500 -s $s -t $t $OUTDIR
    done
  done
done

#Usage: ./runAttacks -a attack -d defense -o victim_order -l victim_location -t victim_attribute -c num_children -m min_chaff -x max_chaff -r num_rounds -s num_samples -e seed -B num_base_blocks_past_min -E num_extra_blocks -W min_left -X max_left -Y min_right -Z max_right <directory>
#      attack values:
#          0 = OtO attack
#          1 = MtO attack
#          2 = MtM attack
#     defense values:
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
#     num_children between 2 and 20
#     min_chaff and max_chaff >= 0
#     num_rounds (for experiment statistical significance) > 0
#     num_samples (for attack statistical significance) > 0
#     seed:  any integer
#     num_base_blocks_past_min >= 0
#     num_extra_blocks >= 0
#     min_left >= 1
#     max_left >= min_left
#     min_right >= 1
#     max_right >= min_right
