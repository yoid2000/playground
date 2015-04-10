#!/bin/bash

gcc -g -o runTests clusters.c hightouch.c buckets.c filters.c runTests.c utilities.c -lm
echo "runTests done"
echo "**********************************************************"

gcc -g -o oldAttacks -D OLD_STYLE_FILTER clusters.c hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
echo "oldAttacks done"
echo "**********************************************************"

gcc -g -o newAttacks clusters.c hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
echo "newAttacks done"
echo "**********************************************************"

gcc -g -o computeOverlap clusters.c computeOverlap.c hightouch.c buckets.c filters.c utilities.c -lm
echo "computeOverlap done"
echo "**********************************************************"

gcc -g -o computeOverlap2 computeOverlap2.c utilities.c -lm
echo "computeOverlap2 done"



#gcc -v -da -Q -o runAttacks hightouch.c buckets.c filters.c attacks.c defenses.c utilities.c -lm
