#!/bin/bash

RESULT_FILE=results.csv
OUT_DIR=results_out
realizability=true
verification=false
synthesis=true
verbose=true
memorylimit=32
timelimit=3600
time_hard=10

#parse command line arguments
POSITIONAL=()
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -t|--timelimit)
    timelimit="$2"
    shift # past argument
    shift # past value
    ;;
    -m|--memorylimit)
    memorylimit="$2"
    shift # past argument
    shift # past value
    ;;
    -b|--benchmark)
    benchmarking=true
    RESULT_FILE=$2
    shift # past argument
    shift # past value
    ;;
    -o|--output)
    OUT_DIR=$2
    shift # past argument
    shift # past value
    ;;
    -r|--realizability)
    synthesis=false
    shift # past argument
    ;;
    -y|--verify)
    verification=true
    shift # past argument
    ;;
    -v|--verbose)
    verbose=true
    shift # past argument
    ;;
    -h|--help)
    show_help=true
    shift # past argument
    ;;
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [ "$show_help" == true ]; then
    echo "Usage: $0 [FLAGS] [OPTIONS] <BENCHMARK_DIR>"
    echo "FLAGS:"
    echo "    -h, --help                Display this help"
    echo "    -r, --realizability       Only check realizability"
    echo "    -y, --verify              Verify the result with the nuXmv model checker"
    echo "    -v, --verbose             Verbose output"
    echo "OPTIONS:"
    echo "    -o, --output <OUT_DIR>    Output all intermediate files to OUT_DIR"
    echo "    -b, --benchmark <RESULTS> Output benchmarking information to RESULTS"
    echo "    -m, --memorylimit <LIMIT> Enforce a memory limit of LIMIT GB"
    echo "    -t, --timelimit <LIMIT>   Enforce a time limit of LIMIT seconds"
    echo "ARGS:"
    echo "    <BENCHMARK_DIR>           The directory containing the benchmark files"
    exit 0
fi

if [[ ${#POSITIONAL[@]} -eq 0 ]]; then
    echo "Error: No benchmark directory specified"
    exit 1
fi

benchmark=${POSITIONAL[0]}

if [[ ! $memorylimit =~ ^[1-9][0-9]*$ ]]; then
    echo "Error: The memory limit has to be a positive integer: $memorylimit"
    exit 1
fi

if [[ ! $timelimit =~ ^[1-9][0-9]*$ ]]; then
    echo "Error: The time limit has to be a positive integer: $timelimit"
    exit 1
fi

if [ ! -d $benchmark ]; then
    echo "Error: The benchmark directory does not exist or is not a folder: $benchmark"
    exit 1
fi

#calucate memory limits from GB to byte
mem_soft=$(($memorylimit * 1024 * 1024))
mem_hard=$(($mem_soft + 1024))

SYFCO=syfco

STRIX=../bin/strix
SMVTOAIG=smvtoaig
COMBINEAIGER=combine-aiger
NUXMV=nuXmv
LTL2SMV=ltl2smv

STRIX_OPTIONS=''
if [ "$verbose" == true ]; then
    STRIX_OPTIONS="$STRIX_OPTIONS -vv --timing"
fi
if [ "$synthesis" == true ]; then
    STRIX_OPTIONS="$STRIX_OPTIONS --auto"
else
    STRIX_OPTIONS="$STRIX_OPTIONS --realizability"
fi

function run_strix {
    LTL_FILE=$1
    INS=$2
    OUTS=$3
    IMPLEMENTATION=$4
    OUT_FILE=$IMPLEMENTATION.out
    runtime="$(date +%s%N)"
    (
        set -o pipefail
        ulimit -S -v $mem_soft
        ulimit -H -v $mem_hard
        echo $STRIX $STRIX_OPTIONS $LTL_FILE --ins "$INS" --outs "$OUTS" -o $IMPLEMENTATION
        timeout -k $time_hard $timelimit $STRIX $STRIX_OPTIONS $LTL_FILE --ins "$INS" --outs "$OUTS" -o $IMPLEMENTATION 2>&1 | tee $OUT_FILE
    ) 2>/dev/null
    result=$?
    runtime=$(($(date +%s%N)-runtime))
    runtime=$(bc -l <<< "scale=2; $runtime/1000000000")
    if [[ $result -eq 0 ]]; then
        if grep -q "^REALIZABLE$" $OUT_FILE; then
            runresult='REALIZABLE';
        elif grep -q "^UNREALIZABLE$" $OUT_FILE; then
            runresult='UNREALIZABLE';
        else
            runresult='UNKNOWN'
        fi
        runstates=$(grep '^Number of env nodes *: *\([0-9]*\)$' $OUT_FILE | head -n 1 | sed -e 's/^Number of env nodes *: *\([0-9]*\)$/\1/')
    elif [[ $result -eq 124 ]] || [[ result -eq 137 ]]; then
        runresult='TIMEOUT'
        runstates='-'
    else
        runresult='ERROR'
        runstates='-'
    fi
}

function run_file {
    SPECIFICATION=$1

    BASE=$(basename ${SPECIFICATION%.tlsf})
    LTL_FILE=$OUT_DIR/$BASE.ltl
    IMPLEMENTATION=$OUT_DIR/$BASE.implementation.aag

    echo -n "$SPECIFICATION" >>$RESULT_FILE

    INS=$(syfco --print-input-signals $SPECIFICATION)
    OUTS=$(syfco --print-output-signals $SPECIFICATION)

    if ! $SYFCO -f ltl -q double -m fully $SPECIFICATION >$LTL_FILE; then
        echo -n ",error_syfco" >>$RESULT_FILE
    else
        run_strix $LTL_FILE "$INS" "$OUTS" $IMPLEMENTATION
        result_realizability=$runresult
        time_realizability=$runtime
        num_states=$runstates
        echo -n ",$result_realizability,$time_realizability,$num_states" >>$RESULT_FILE
        if [ "$synthesis" == true ]; then
            if [ "$result_realizability" == "REALIZABLE" ] || [ "$result_realizability" == "UNREALIZABLE" ]; then
                # get size of aiger circuit
                num_latches=$(head -n 1 $IMPLEMENTATION | cut -d ' ' -f 4)
                num_gates=$(head -n 1 $IMPLEMENTATION | cut -d ' ' -f 6)
                size_aiger=$(($num_latches + $num_gates))
                echo -n ",$size_aiger" >>$RESULT_FILE
                if  [ "$verification" == true ]; then
                    runtime_verification="$(date +%s%N)"
                    result_verification=$(../scripts/verify.sh $IMPLEMENTATION $FILE $result_realizability $timelimit)
                    runtime_verification=$(($(date +%s%N)-runtime_verification))
                    runtime_verification=$(bc -l <<< "scale=2; $runtime_verification/1000000000")
                    time_verification=$runtime
                    echo -n ",$result_verification,$runtime_verification" >>$RESULT_FILE
                fi
            else
                echo -n ",UNKNOWN,0" >>$RESULT_FILE
            fi
        fi
    fi
    echo >>$RESULT_FILE
}

echo -n "file,result_realizability,time_strix,states_strix" >$RESULT_FILE
if [ "$synthesis" == true ]; then
    echo -n ",size_aiger" >>$RESULT_FILE
fi
if [ "$verification" == true ]; then
    echo -n ",result_verification,time_verification" >>$RESULT_FILE
fi
echo >>$RESULT_FILE

mkdir -p $OUT_DIR

for FILE in $(find $benchmark -mindepth 1 -type f -name "*.tlsf" | sort); do
    echo "Benchmarking $FILE"
    run_file $FILE
done
