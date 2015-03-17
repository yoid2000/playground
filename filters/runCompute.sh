#!/bin/bash

DIR=$(pwd)
# OUTDIR="/home/francis/gnuplot/computeOverlap"
OUTDIR="/root/paul/computeOverlap"
MAXBUCKET=2000
EXPNUMSAMP=4

for SizeRatioMin in 1.00
do
for SizeRatioMax in 1.50
do
  ./computeOverlap $MAXBUCKET $EXPNUMSAMP $SizeRatioMax $SizeRatioMax $OUTDIR
  ./makeGnuplot.sh $MAXBUCKET $EXPNUMSAMP $SizeRatioMax $SizeRatioMax $OUTDIR
done
done
