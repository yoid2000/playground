#!/bin/bash

DIR=$(pwd)
#OUTDIR="/home/francis/attacks/"
OUTDIR="./"

D_OTO=1
D_MTO=2
D_MTM=3
VFIRST=0
ALL_CHILD=2
ALL_LEFT=3
ALL_RIGHT=4
ATT_YES=0
ATT_NO=1
ROUNDS="-a"

for BASE in 2
do
for SIDE in $ALL_RIGHT # $ALL_LEFT
do
for RIGHT in 3
do
for LEFT in 3
do
for VIC in $ATT_YES # $ATT_NO
do
for SAMP in 20 # 20 40 80
do
 ./runAttacks -l $LEFT -r $RIGHT -d $D_MTM -o $VFIRST -v $SIDE -m 0 -x 0 $ROUNDS 10 -s $SAMP -t $VIC -u 200 -B $BASE -L 0 -R 0 -e 1 $OUTDIR 
done
done
done
done
done
done

#Usage: ./runAttacks -l min_left -r min_right -L max_left -R max_right -d defense -o victim_order -v victim_location -t victim_attribute -m min_chaff -x max_chaff -a num_rounds -s num_samples -e seed -B num_base_blocks_past_min -u user_per_bucket <directory>
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
