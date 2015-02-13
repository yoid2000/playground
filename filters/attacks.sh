#!/bin/bash

DIR=$(pwd)
OUTDIR="/home/francis/attacks/"

./runAttacks -a 1 -d 3 -o 0 -l 0 -t 0 -c 2 -m 0 -x 0 -r 40 -s 10 -e 1 $OUTDIR
./runAttacks -a 1 -d 3 -o 0 -l 0 -t 1 -c 2 -m 0 -x 0 -r 40 -s 10 -e 1 $OUTDIR

#
#Usage: ./runAttacks -a attack -d defense -o victim_order -l victim_location -t victim_attribute -c num_children -m min_chaff -x max_chaff -r num_rounds -s num_samples, -e seed
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
#          2 = victim random
#     victim_location values:
#          0 = victim in parent
#          1 = victim in one child
#          2 = victim in all children
#     victim_attribute values:
#          0 = victim has attribute
#          1 = victim does not have attribute
#     num_children between 2 and 20
#     min_chaff and max_chaff >= 0
#     num_rounds (for experiment statistical significance) > 0
#     num_samples (for attack statistical significance) > 0
#     seed:  any integer
