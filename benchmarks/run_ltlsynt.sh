#!/bin/bash

#default arguments

timelimit=3600
memorylimit=32
benchmark=false
out_dir=/tmp
output_file=/dev/null
result_file=/dev/null
output_aiger=false
only_realizability=false
synthesis=true
verification=false
verbose=false
show_help=false

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
    result_file=$2
    shift # past argument
    shift # past value
    ;;
    -o|--output)
    out_dir=$2
    shift # past argument
    shift # past value
    ;;
    -a|--aiger)
    output_aiger=true
    shift # past argument
    ;;
    -r|--realizability)
    only_realizability=true
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
    echo "Usage: $0 [FLAGS] [OPTIONS] <TLSF_FILE>"
    echo "FLAGS:"
    echo "    -h, --help                Display this help"
    echo "    -r, --realizability       Only check realizability"
    echo "    -y, --verify              Verify the result with the nuXmv model checker"
    echo "    -v, --verbose             Verbose output"
    echo "    -a, --aiger               Output the controller as an AIGER circuit"
    echo "OPTIONS:"
    echo "    -o, --output <OUT_DIR>    Output all intermediate files to OUT_DIR"
    echo "    -b, --benchmark <RESULTS> Output benchmarking information to RESULTS"
    echo "    -m, --memorylimit <LIMIT> Enforce a memory limit of LIMIT GB"
    echo "    -t, --timelimit <LIMIT>   Enforce a time limit of LIMIT seconds"
    echo "ARGS:"
    echo "    <TLSF_FILE>               Sets the input file in tlsf format to use"
    exit 0
fi

if [[ ${#POSITIONAL[@]} -eq 0 ]]; then
    echo "Error: no input file specified"
    exit 1
fi
input_file=${POSITIONAL[0]}

BASE_FILE=$(basename ${input_file%.tlsf})

# output files
LTL_FILE=$out_dir/$BASE_FILE.ltl
AIG_FILE=$out_dir/$BASE_FILE.aig
SMV_FILE=$out_dir/$BASE_FILE.smv
MONITOR_FILE=$out_dir/$BASE_FILE.monitor.aag
COMBINED_FILE=$out_dir/$BASE_FILE.combined.aag

#calucate memory limits from GB to byte
mem_soft=$(($memorylimit * 1024 * 1024))
mem_hard=$(($mem_soft + 1024))

# tool paths
LTLSYNT=ltlsynt
LTLSYNT_OPTIONS="--algo=ds"
if [ "$only_realizability" == true ]; then
    LTLSYNT_OPTIONS="$LTLSYNT_OPTIONS --realizability"
elif [ "$output_aiger" == true ]; then
    LTLSYNT_OPTIONS="$LTLSYNT_OPTIONS --aiger"
fi

SYFCO=syfco
SYFCO_OPTIONS='-os mealy'
SMVTOAIG=smvtoaig
COMBINEAIGER=combine-aiger
NUXMV=nuXmv
LTL2SMV=ltl2smv

mem_soft=$((32 * 1024 * 1024))
mem_hard=$(($mem_soft + 1024))

function size_of_aig {
    num_latches=$(head -n 1 $1 | cut -d ' ' -f 4)
    num_gates=$(head -n 1 $1 | cut -d ' ' -f 6)
    size=$(($num_latches + $num_gates))
}

function run_ltlsynt {
    INPUT=$1
    INS=$2
    OUTS=$3
    OUTPUT=$4
    OUT_FILE=$out_dir/$(basename $4).out
    runtime="$(date +%s%N)"
    (
        set -o pipefail
        ulimit -S -v $mem_soft
        ulimit -H -v $mem_hard
        echo timeout -k 10 $timelimit $LTLSYNT $LTLSYNT_OPTIONS -F $INPUT --ins=\"$INS\" --outs=\"$OUTS\"
        timeout -k 10 $timelimit $LTLSYNT $LTLSYNT_OPTIONS -F $INPUT --ins="$INS" --outs="$OUTS" 2>&1 | tee $OUT_FILE
    ) 2>/dev/null
    result=$?
    runtime=$(($(date +%s%N)-runtime))
    runtime=$(bc -l <<< "scale=2; $runtime/1000000000")
    if [[ result -eq 0 ]] || [[ result -eq 1 ]]; then
        if grep -q "^REALIZABLE$" $OUT_FILE; then
            runresult='realizable';
            tail -n+2 $OUT_FILE >$OUTPUT
        elif grep -q "^UNREALIZABLE$" $OUT_FILE; then
            runresult='unrealizable';
        else
            runresult='unknown'
        fi
    elif [[ $result -eq 124 ]] || [[ result -eq 137 ]]; then
        runresult='timeout'
    else
        runresult='error'
    fi
    return $result
}

function run_nuxmv_single {
    INPUT=$1
    OUT_FILE=$out_dir/$(basename $2).out
    COMMAND=$3
    (
        set -o pipefail
        ulimit -S -v $mem_soft
        ulimit -H -v $mem_hard
        if [ "$verbose" == true ]; then
            echo "read_aiger_model -i $INPUT; $COMMAND; quit" | timeout -k 10 $timelimit $NUXMV -int 2>&1 | tee $OUT_FILE
        else
            echo "read_aiger_model -i $INPUT; $COMMAND; quit" | timeout -k 10 $timelimit $NUXMV -int >$OUT_FILE 2>&1
        fi
    ) 2>/dev/null
    if [[ $result -eq 0 ]]; then
        if grep -q "specification.*is true" $OUT_FILE; then
            return 0
        elif grep -q "specification.*is false" $OUT_FILE; then
            return 1
        else
            return 2
        fi
    fi
}

function run_nuxmv_parallel {
    if [ "$verbose" == true ]; then
        echo "Verifying controller"
    fi
    INPUT=$1
    OUT_FILE=$1.out
    runtime="$(date +%s%N)"

    run_nuxmv_single $INPUT $OUT_FILE.2 'encode_variables; build_boolean_model; check_ltlspec_klive'
    r1=$?
    if [[ $r1 -eq 0 ]]; then
        runresult='passed';
    elif [[ $r1 -eq 1 ]]; then
        runresult='failed';
    else
        runresult='unknown';
    fi
    runtime=$(($(date +%s%N)-runtime))
    runtime=$(bc -l <<< "scale=2; $runtime/1000000000")
}

function verify_result {
    if [ -f $AIG_FILE ]; then
        if ! $SYFCO $SYFCO_OPTIONS -f smv -m fully $input_file >$SMV_FILE; then
            result_verification='error_tlsf_to_smv'
            return 1
        fi
        if ! $SMVTOAIG -L $LTL2SMV $SMV_FILE >$MONITOR_FILE 2>/dev/null; then
            result_verification='error_smv_to_aig'
            return 1
        fi
        if ! $COMBINEAIGER $MONITOR_FILE $AIG_FILE >$COMBINED_FILE; then
            result_verification='error_combine_aiger'
            return 1
        fi
        run_nuxmv_parallel $COMBINED_FILE
        time_verification=$runtime
        result_verification=$runresult
        if [[ "$result_verification" == 'passed' ]]; then
            echo "Controller is verified!"
        elif [[ "$result_verification" == 'failed' ]]; then
            echo "Counterexample for controller found!"
        else
            echo "Controller could not be verified within the time limit!"
        fi
    fi
}

if [ "$benchmarking" == true ]; then
    echo -n "$input_file" >>$result_file
fi

syfco -f ltlxba -m fully -o $LTL_FILE $input_file
INS=$(syfco -ins $input_file | sed -e 's/ //g')
OUTS=$(syfco -outs $input_file | sed -e 's/ //g')

run_ltlsynt $LTL_FILE "$INS" "$OUTS" $AIG_FILE

if [ "$benchmarking" == true ]; then
    echo -n ",$runresult,$runtime" >>$result_file
    if [ "$output_aiger" == true ]; then
        size_of_aig $AIG_FILE
        echo -n ",$size" >>$result_file
    fi
fi
if [ "$verification" == true ]; then
    verify_result
    if [ "$benchmarking" == true ]; then
        echo -n ",$result_verification,$time_verification" >>$result_file
    fi
fi

if [ "$benchmarking" == true ]; then
    echo >>$result_file
fi
