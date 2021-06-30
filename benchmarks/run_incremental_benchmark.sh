#!/bin/bash

RESULT_FILE=results.csv
OUT_DIR=results_out
memorylimit=32
timelimit=3600

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
    *)    # unknown option
    POSITIONAL+=("$1") # save it in an array for later
    shift # past argument
    ;;
esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

if [[ ${#POSITIONAL[@]} -eq 0 ]]; then
    echo "Error: No benchmark directory specified"
    exit 1
fi

benchmark=${POSITIONAL[0]}

SYFCO=syfco
STRIX=../bin/strix

STRIX_OPTIONS='-v --timing --realizability'

function run_strix {
    BASIC_TLSF_FILE1=$1
    BASIC_TLSF_FILE2=$2
    OUT_FILE=$BASIC_TLSF_FILE1-$(basename $BASIC_TLSF_FILE2).out

    runtime="$(date +%s%N)"
    (
        set -o pipefail
        ulimit -S -v $mem_soft
        ulimit -H -v $mem_hard
        echo $STRIX $STRIX_OPTIONS $BASIC_TLSF_FILE1 $BASIC_TLSF_FILE2 2>&1 | tee $OUT_FILE
        timeout $timelimit $STRIX $STRIX_OPTIONS $BASIC_TLSF_FILE1 $BASIC_TLSF_FILE2 2>&1 | tee $OUT_FILE
    ) 2>/dev/null
    result=$?
    runtime=$(($(date +%s%N)-runtime))
    runtime=$(bc -l <<< "scale=2; $runtime/1000000000")
    time1=0
    time2=0
    if [[ $result -eq 0 ]]; then
        time1=$(grep 'Finished checking realizability, took \(.*\) seconds' $OUT_FILE | head -n 1 | sed -e 's/^.*took \(.*\) seconds.*$/\1/')
        time2=$(grep 'Finished checking realizability, took \(.*\) seconds' $OUT_FILE | tail -n 1 | sed -e 's/^.*took \(.*\) seconds.*$/\1/')
        runresult='passed';
        runresult='success'
    elif [[ $result -eq 124 ]] || [[ result -eq 137 ]]; then
        runresult='timeout'
    else
        runresult='error'
    fi
}

function run_files {
    FILE1=$1
    FILE2=$2

    BASE_FILE1=$(basename $FILE1)
    BASE_FILE2=$(basename $FILE2)

    BASE1=${BASE_FILE1%.tlsf}
    BASE2=${BASE_FILE2%.tlsf}

    BASIC_TLSF_FILE1=$OUT_DIR/$BASE1.basic.tlsf
    BASIC_TLSF_FILE2=$OUT_DIR/$BASE2.basic.tlsf
    CONTROLLER_FILE=$OUT_DIR/$BASE.controller.aag
    echo -n "$BASE_FILE1,$BASE_FILE2" >>$RESULT_FILE

	if ! $SYFCO -os mealy -f basic -m fully $FILE1 >$BASIC_TLSF_FILE1; then
        echo -n ",error_syfco" >>$RESULT_FILE
    elif ! $SYFCO -os mealy -f basic -m fully $FILE2 >$BASIC_TLSF_FILE2; then
        echo -n ",error_syfco" >>$RESULT_FILE
    else
        run_strix $BASIC_TLSF_FILE1 $BASIC_TLSF_FILE2
        echo -n ",$runresult,$runtime,$time1,$time2" >>$RESULT_FILE
    fi
    echo >>$RESULT_FILE
}

echo "file1,file2,result,time_total,time_file1,time_file2" >$RESULT_FILE
for DIR in $(find $benchmark -type d | sort); do
    echo "Searching directory $DIR"
    for FILE1 in $(find $DIR -mindepth 1 -maxdepth 1 -type f -name "*.tlsf" | sort); do
        for FILE2 in $(find $DIR -mindepth 1 -maxdepth 1 -type f -name "*.tlsf" | sort); do
            if [ $FILE1 != $FILE2 ]; then
                echo "Benchmarking $FILE1 $FILE2"
                run_files $FILE1 $FILE2
            fi
        done
    done
done
