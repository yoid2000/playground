#!/bin/bash

DIR=$(pwd)
OUTDIR="/home/francis/gnuplot/computeOverlap/newComputeOverlap"
# OUTDIR="/root/paul/computeOverlap"
MINBUCKET=2
MAXBUCKET=2000
EXPNUMSAMP=6

for SizeRatioMin in 1.00
do
for SizeRatioMax in 16.00
do
  ./computeOverlap $MINBUCKET $MAXBUCKET $EXPNUMSAMP $SizeRatioMin $SizeRatioMax $OUTDIR
  echo "./computeOverlap $MINBUCKET $MAXBUCKET $EXPNUMSAMP $SizeRatioMin $SizeRatioMax $OUTDIR"
  ./makeGnuplot.sh $MINBUCKET $MAXBUCKET $EXPNUMSAMP $SizeRatioMin $SizeRatioMax $OUTDIR
done
done
