#!/bin/bash

source ./banner.sh
DICT="Test/passwords/phpbb_ascii_5-16_sorted.txt"
ACT="Test/passwords/rockyou_ascii_5_16_sorted.txt"
MODEL="Test/MODELs/PasswordPro_SMALL/"
EX="./bin/AdamAttack"
OUT_PLOT_IMG="adams_vs_standard_test.png"
MAXG=9



mkdir -p LOGs
cd ..

echo "We will perform 10^$MAXG guesses at most. It will take a while, but it should be fast..."

# adam attack
echo "RUNNING ADaMs attack..."
$EX -a 9 --max-guesses-pow $MAXG -w $DICT --config-dir $MODEL --hashes $ACT 2> Test/LOGs/adam.txt 1>/dev/null
cat Test/LOGs/adam.txt

# standard attack
echo "RUNNING Standard attack...."
$EX -a 0 --max-guesses-pow $MAXG -w $DICT --config-dir $MODEL --hashes $ACT 2> Test/LOGs/standard.txt 1>/dev/null

echo "Plot results in $OUT_PLOT_IMG..."
python Test/plot.py Test/LOGs/adam.txt Test/LOGs/standard.txt Test/$OUT_PLOT_IMG




