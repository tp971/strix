#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

LTL=$(syfco -f ltl -q double -m fully $1)
INS=$(syfco -f ltl --print-input-signals $1)
OUTS=$(syfco -f ltl --print-output-signals $1)

echo $DIR/../bin/strix -f "$LTL" --ins "$INS" --outs "$OUTS" ${@:2}
$DIR/../bin/strix -f "$LTL" --ins "$INS" --outs "$OUTS" ${@:2}
