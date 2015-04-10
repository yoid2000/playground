
# some constants
$NUM_PROCS = 5;
$NUM_THORS = 10;
$FIRST_THOR = 50;
$LAST_THOR = $FIRST_THOR + $NUM_THORS;

$D_NONE = 0;
$D_BASIC = 1;
$D_OTO = 2;
$D_MTO = 3;
$D_MTM = 4;
$A_GEN = 0;
$A_PER = 1;
$A_BAR = 2;
$A_NUN = 3;
$A_NUNBAR = 4;
$ALL_RIGHT = 4;
$ONE_RIGHT = 5;
$OUTDIR = "\/root\/paul\/attacks\/";


open($lf, '>', "launchAttacks.sh");
open($kf, '>', "killAttacks.sh");
print $lf "#!/bin/bash\n\n";
print $kf "#!/bin/bash\n\n";

($thor, $proc, $file) = initLoop();
while(1) {
  $fileName = makeFileName($thor, $proc);
  print $file "#!/bin/bash\n\n";
  print $lf "scp -i ~/.ssh/id_rsa_root attacks", $thor, ".", $proc, ".sh root\@thor", $thor, ":paul/", $fileName, "\n";
  print $lf "ssh -i ~/.ssh/id_rsa_root root\@thor", $thor, " \'chmod 777 paul/att*; nohup paul/", $fileName, " > /dev/null 2> /dev/null < /dev/null &\'\n";
  print $kf "ssh -i ~/.ssh/id_rsa_root root\@thor", $thor, " \'kill -9 \$(pidof -x ", $fileName, ")\'\n";
  print $kf "ssh -i ~/.ssh/id_rsa_root root\@thor", $thor, " \'kill -9 \$(pidof newAttacks)\'\n";
  print $kf "ssh -i ~/.ssh/id_rsa_root root\@thor", $thor, " \'kill -9 \$(pidof oldAttacks)\'\n";
  ($thor, $proc, $file) = nextLoop($thor, $proc, $file);
  if (loopDone($thor, $proc)) {
    last;
  }
}
close($lf);

@defense = ($D_MTM, $D_BASIC);              # -d
@order = (0,1);                             # -o
@users = (100, 400, 1600);                  # -u
@side = ($ALL_RIGHT, $ONE_RIGHT);           # -v
@right = (2, 5, 16);                        # -r
@left = (2, 5, 16);                         # -l

($thor, $proc, $file) = initLoop();

$c = $A_GEN;
foreach $d (@defense) {
foreach $o (@order) {
foreach $u (@users) {
foreach $v (@side) {
foreach $r (@right) {
foreach $l (@left) {
  if (($r <= 5) && ($l <= 5)) {      # smallish clusters
    foreach $B (0, 4) {
      if ($v == $ALL_RIGHT) {
        foreach $s (10, 20, 40, 80) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
      if ($v == $ONE_RIGHT) {
        foreach $s (40, 80, 160) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
    }
  }
  else {                      # largish clusters
    foreach $B (0, 8) {
      if ($v == $ALL_RIGHT) {
        foreach $s (20, 40, 80) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
      if ($v == $ONE_RIGHT) {
        foreach $s (80, 160) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
    }
  }
}}}}}}

$d = $D_MTM;

$c = $A_PER;
foreach $o (@order) {
foreach $v (@side) {
foreach $l (@left) {
  $r = $l;
  if (($r <= 5) && ($l <= 5)) {      # smallish clusters
    foreach $u (@users) {
      if ($v == $ALL_RIGHT) {
        foreach $s (10, 20, 40, 80) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
      if ($v == $ONE_RIGHT) {
        foreach $s (40, 80, 160) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
    }
  }
  else {                      # largish clusters
    foreach $u (400, 1600) {
      if ($v == $ALL_RIGHT) {
        foreach $s (20, 40, 80) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
      if ($v == $ONE_RIGHT) {
        foreach $s (80, 160) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
    }
  }
}}}

$c = $A_BAR;
foreach $o (@order) {
foreach $u (400, 1600) {
foreach $v (@side) {
foreach $l (@left) {
  $r = $l;
  if (($r <= 5) && ($l <= 5)) {      # smallish clusters
      if ($v == $ALL_RIGHT) {
        foreach $s (10, 20, 40, 80) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
      if ($v == $ONE_RIGHT) {
        foreach $s (40, 80, 160) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
  }
  else {                      # largish clusters
      if ($v == $ALL_RIGHT) {
        foreach $s (20, 40, 80) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
      if ($v == $ONE_RIGHT) {
        foreach $s (80, 160) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
  }
}}}}


$B = 0;  # ignored
foreach $c ($A_NUN, $A_NUNBAR) {
foreach $o (@order) {
foreach $u (@users) {
foreach $v (@side) {
  if ($c == $A_NUN) {
    $r = 3; $l = 3;
  }
  if ($c == $A_NUNBAR) {
    $r = 2; $l = 3;
  }
      if ($v == $ALL_RIGHT) {
        foreach $s (10, 20, 40, 80) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
      if ($v == $ONE_RIGHT) {
        foreach $s (20, 40, 80, 160) {
          ($thor, $proc, $file) = printTwoAttacks($thor, $proc, $file, 
                                         $c, $d, $o, $u, $v, $r, $l, $B, $s);
        }
      }
}}}}


sub initLoop {
  my($file);
  my($thor, $proc) = ($FIRST_THOR, 0);
  $fileName = makeFileName($thor, $proc);
  open($file, '>>', $fileName);
  return($thor, $proc, $file);
}

sub loopDone {
my($thor, $proc) = @_;
  if (($thor == $FIRST_THOR) && ($proc == 0)) {
    return(1);
  }
  else {
    return(0);
  }
}

sub makeFileName {
my($thor, $proc) = @_;
  return("attacks".$thor.".".$proc.".sh");
}

sub nextLoop {
my($thor, $proc, $file) = @_;
  my($fileName);

  close($file);
  ++$proc;
  if ($proc >= $NUM_PROCS) {
    $proc = 0;
    ++$thor;
    if ($thor >= $LAST_THOR) {
      ($thor, $proc) = initLoop();
    }
  }
  $fileName = makeFileName($thor, $proc);
  open($file, '>>', $fileName);
  return($thor, $proc, $file);
}

sub printTwoAttacks {
my($thor, $proc, $file, $c, $d, $o, $u, $v, $r, $l, $B, $s) = @_;

  print $file "/root/paul/filters/newAttacks -c ", $c, " -l ", $l, " -r ", $r, " -d ", $d, " -o ", $o, " -v ", $v, " -m 0 -x 0 -a 100 -s ", $s, " -u ", $u, " -B ", $B, " -L 0 -R 0 -e 1 ", $OUTDIR, "\n";
  ($thor, $proc, $file) = nextLoop($thor, $proc, $file);

  print $file "/root/paul/filters/oldAttacks -c ", $c, " -l ", $l, " -r ", $r, " -d ", $d, " -o ", $o, " -v ", $v, " -m 0 -x 0 -a 100 -s ", $s, " -u ", $u, " -B ", $B, " -L 0 -R 0 -e 1 ", $OUTDIR, "\n";
  ($thor, $proc, $file) = nextLoop($thor, $proc, $file);

  return($thor, $proc, $file);
}
