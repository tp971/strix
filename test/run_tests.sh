#!/bin/bash

# exit on error
set -e
# echo commands
set -x
# break when pipe fails
set -o pipefail

# tool paths
STRIX=$1
ROOT_DIR=$2

# input file
SPECIFICATION=$3

# type of test
TEST=$4

# temporary files
BASE=$(basename ${2%.tlsf})
IMPLEMENTATION=/tmp/$BASE.aag

STRIX_OPTIONS='--validate-jni -e pq -c --auto'

# get formula, inputs and outputs from specification using syfco
LTL=$(syfco -f ltl -q double -m fully $SPECIFICATION)
INS=$(syfco --print-input-signals $SPECIFICATION)
OUTS=$(syfco --print-output-signals $SPECIFICATION)

# run the tool
RESULT=$($STRIX $STRIX_OPTIONS -f "$LTL" --ins "$INS" --outs "$OUTS" -o $IMPLEMENTATION)

if [ "$TEST" == "REALIZABLE" ] || [ "$TEST" == "UNREALIZABLE" ]; then
    # check if tool answers correctly (and nothing else)
    if [ "$TEST" != "$RESULT" ]; then
        echo "Incorrect result for $TEST: $RESULT"
        exit 1
    fi

    # verify solution (with time limit of 10 seconds)
    $ROOT_DIR/scripts/verify.sh $IMPLEMENTATION $SPECIFICATION $TEST 10

    # remove solution
    rm -f $IMPLEMENTATION
else
    # unknown test
    echo "Unknown test: $TEST"
    exit 2
fi
