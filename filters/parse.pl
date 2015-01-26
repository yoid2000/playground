# I want an array of size [800][256].
# I want values between 0 and 63 (i.e. no overlap to complete overlap)
# I want 4 "confidence" numbers:
#    0 = 0<=SD<=2,
#    1 = 3<=SD<=7,
#    2 = 8<=SD<=16,
#    3 = 17<=SD

$numDiffOutOfRange = 0;
$numAllOutOfRange = 0;
$maxOverlap = 0;
$values[800][255] = 0;  # simply sets array size

for ($i = 0; $i < 800; $i++) {
  for ($j = 0; $j < 256; $j++) {
    $values[$i][$j] = 64;   # indicates no value assigned
  }
}

open($gf, '>', "gnuarray.data");
open($cf, '>', "carray.data");
open($ff, '>', "final-gnuarray.data");
open($nf, '>', "filled-gnuarray.data");

$numGreater = 0;
while (<>) {
  chomp;
  # print $_;
  # numFIrst is x axis, $numCommon is y axis, overlap_av is the value
  ($numFirst, $numCommon, $numSamples, $overlap_av, $overlap_sd, $bsize1_sd, $level_sd, $overlap_min, $overlap_max) = split(/ /,$_);

  $diff = ($numFirst - $numCommon);
  if ($diff > 255) {
    $diff = 255;
    $numDiffOutOfRange++;
  }
  if ($numFirst > 800) {
    $numFirst = 800;
    $numAllOutOfRange++;
  }
  $val = int(($overlap_av * 63) / 100);

  if ($val > 63) {
    $val = 63;     # should never happen
  }
  print $gf $numFirst, " ", $diff, " ", $val, " ";
  $sd = int($overlap_sd);

  if ($sd <= 2) {
    # do nothing on purpose
    print $gf "0\n";
  }
  elsif ($sd <= 7) {
    print $gf "1\n";
    # $val += 64;  # set 0x40 bit
  }
  elsif ($sd <= 16) {
    print $gf "2\n";
    # $val += 128;  # set 0x80 bit
  }
  else {
    print $gf "3\n";
    # $val += 192;  # set 0xc0 bits
  }

  $values[$numFirst][$diff] = $val;
}
# Now fill in the not yet written array locations
for ($j = 0; $j < 256; $j++) {
  # label first and last filled entry
  for ($i = 0; $i < 800; $i++) {
    if ($values[$i][$j] != 64) {
      $first = $i;
      last;
    }
  }
  for ($i = (800-1); $i >= 0; $i--) {
    if ($values[$i][$j] != 64) {
      $last = $i;
      last;
    }
  }
  # fill in everything left of $first with 0 (no overlap)
  for ($i = 0; $i < $first; $i++) {
    $values[$i][$j] = 0;
    print $nf $i, " ", $j, " 0\n";
  }
  # fill in everything right of $last with the value of $last
  $fill = $values[$last][$j];
  for ($i = ($last+1); $i < 800; $i++) {
    $values[$i][$j] = $fill;
    print $nf $i, " ", $j, " ", $fill, "\n";
  }
  # fill in everything in between with the value to the left
  for ($i = $first; $i < $last; $i++) {
    if ($values[$i][$j] != 64) {
      $fill = $values[$i][$j];
    }
    else {
      $values[$i][$j] = $fill;
      print $nf $i, " ", $j, " ", $fill, "\n";
    }
  }
}

# create the initialized C structure
print $cf "unsigned char overlap_array[800][256] {\n";
for ($i = 0; $i < 800; $i++) {
  print $cf "  {    // [", $i, "][0]\n    ";
  $k = 0;
  for ($j = 0; $j < 256; $j++) {
    if ($j == (256-1)) {
      print $cf $values[$i][$j], "\n";
    }
    else {
      print $cf $values[$i][$j], ", ";
    }
    if (++$k > 10) {
      $k = 0;
      print $cf "\n    ";
    }
  }
  if ($i == (800-1)) {
    print $cf "  }\n";
  }
  else {
    print $cf "  },\n";
  }
}
print $cf "}\n";

# now write out the full values array for gnuplot
for ($i = 0; $i < 800; $i++) {
  for ($j = 0; $j < 256; $j++) {
    print $ff $i, " ", $j, " ", $values[$i][$j], "\n";
  }
}

print $numDiffOutOfRange, " entries had too much diff, truncated to 255\n";
print $numAllOutOfRange, " entries had too big x axis, truncated to 800\n";
