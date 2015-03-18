#!/bin/bash

# ....scatter.data:
# bsize1 bsize2 numFirst numSecond level 0:1 1:1 0:0 sizeRatio trueOverlap computedOverlap overlapDiff

# .....overlapDiff.data
# computed_overlap diff_av diff_sd diff_min diff_max

# .....base.data
# numFirst numCommon numSamples overlap_av overlap_sd bsize1_sd level_sd overlap_min overlap_max



minBucket=$1
maxBucket=$2
numSamplesExp=$3
sizeRatioMin=$4
sizeRatioMax=$5
outDir=$6

DIR=$(pwd)

cd $outDir

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Small Bucket Size (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.bsize1.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:1 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Large Bucket Size (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.bsize2.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:2 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Number of Ones, Large Bucket (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.numSecond.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:1000]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:4 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Filter Level (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.level.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:5]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:5 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Num Zero/One Pairs (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.zo.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:1024]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:6 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Num Zero/Zero Pairs (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.zz.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:1024]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:8 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Bucket Size Ratio (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.sizeRatio.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:16]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:9 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Groudtruth Overlap (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.trueOverlap.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:100]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:10 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Computed Overlap (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.computedOverlap.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:64]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:11 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Diff Overlap (true-computed) (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.diffOverlapBase.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [-70:70]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:7:12 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Diff Real - Computed Overlap (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.diffOverlap.jpg\"" >> plotCommands
echo "set xlabel  \"Real Overlap\"" >> plotCommands
echo "set ylabel \"Real - Computed Overlap\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 10:12 with dots notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Bucket Size Ratio (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.12.sizeRatio.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Ones, Big Bucket\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:16]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:4:9 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Bucket Size, Large Bucket (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.12.bsize2.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Ones, Big Bucket\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 3:4:2 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Diff Real - Computed Overlap (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.diffOverlap2.jpg\"" >> plotCommands
echo "set xlabel  \"Computed Overlap\"" >> plotCommands
echo "set ylabel \"Real - Computed Overlap\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.scatter.data\" using 11:12 with dots notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set title \"Diff Real - Computed Overlap (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.diffOverlapLines.jpg\"" >> plotCommands
echo "set xlabel  \"Computed Overlap\"" >> plotCommands
echo "set ylabel \"Real - Computed Overlap\"" >> plotCommands
dataFile=$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.overlapDiff.data
echo "plot \"$dataFile\" using 1:2 title \"av\" with lines, \"$dataFile\" using 1:3 title \"sd\" with lines, \"$dataFile\" using 1:4 title \"min\" with lines, \"$dataFile\" using 1:5 title \"max\" with lines" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Overlap Estimates, Standard Deviation (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.overlap.sd.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:50]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.0.base.data\" using 1:2:5 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Overlap Estimates, Average (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.overlap.av.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:100]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.0.base.data\" using 1:2:4 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Overlap Estimates, Min (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.overlap.min.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:100]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.0.base.data\" using 1:2:8 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Overlap Estimates, Max (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.overlap.max.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:100]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.0.base.data\" using 1:2:9 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Small Bucket Size, SD (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.bsize1.sd.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.0.base.data\" using 1:2:6 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Filter Level, SD (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.level.sd.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "set cbrange [0:2]" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.0.base.data\" using 1:2:7 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#-------------------------------------

echo "set terminal jpeg" > plotCommands
echo "set palette defined ( 0 \"green\", 1 \"red\", 2 \"blue\")" >> plotCommands
echo "set title \"Number of Samples (Max Bucket $maxBucket, Size Ratio ($sizeRatioMin, $sizeRatioMax))\"" >> plotCommands
echo "set output \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.samples.jpg\"" >> plotCommands
echo "set xlabel  \"Number of Ones, Small Bucket\"" >> plotCommands
echo "set ylabel \"Number of Common Ones\"" >> plotCommands
echo "set pointsize 1" >> plotCommands
echo "plot \"$minBucket.$maxBucket.$numSamplesExp.$sizeRatioMin.$sizeRatioMax.0.base.data\" using 1:2:3 with dots palette notitle" >> plotCommands
gnuplot plotCommands

#------------------------------
