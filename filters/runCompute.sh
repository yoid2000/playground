#!/bin/bash

DIR=$(pwd)
OUTDIR="/home/francis/gnuplot/computeOverlap/smallRatio"
# OUTDIR="/root/paul/computeOverlap"
MINBUCKET=2
MAXBUCKET=2000
EXPNUMSAMP=4

for SizeRatioMin in 1.00
do
for SizeRatioMax in 1.50
do
  ./computeOverlap $MINBUCKET $MAXBUCKET $EXPNUMSAMP $SizeRatioMin $SizeRatioMax $OUTDIR
  ./makeGnuplot.sh $MINBUCKET $MAXBUCKET $EXPNUMSAMP $SizeRatioMin $SizeRatioMax $OUTDIR
done
done
