#!/bin/bash

bin_dir=`dirname $0`
TOPGEN="${bin_dir}/mrnet_topgen"
topology_dir="${bin_dir}/mrnet_test_files"

if [ ! -d $topology_dir ]; then
    topology_dir="${bin_dir}/topology_files"
    if [ ! -d $topology_dir ]; then
        echo "Error: could not locate directory containing topology files"
	exit
    fi
fi

topologies=(1x1 1x4 1x16 1x1x2 1x2x4 1x4x16 1x1x1x1)
remote_topology_specs=(1 4 16 1:2 2:2x2 4:4x4 1:1:1)

usage="Usage: $0 ( -l | -r hostfile | -a hostfile ) [ -filter ] [ -lightweight ]"
print_usage()
{
    echo $usage
    return 0
}

print_help()
{
    echo $usage
    echo " One of the following must be provided:"
    echo "    -l           : run tests on the local host only"
    echo "    -r           : run tests on remote hosts"
    echo "    -a           : run tests locally and remotely"
    echo " Optional switches:"
    echo "    -filter      : test dynamic filter loading"
    echo "    -lightweight : additionally run tests using lightweight back-ends"
    return 0
}

create_remote_topologies()
{
    # $1, 1st arg, is the hostfile mapping localhost:n => remotehost
    remote_topology_prefix="remote"

    for (( idx = 0 ; idx < ${#topologies[@]}; idx++ ));
    do
	remote_topology="$remote_topology_prefix-${topologies[$idx]}.top"

        echo -n "Creating \"$remote_topology\" ... "

        /bin/rm -f $remote_topology

        $TOPGEN -t g:${remote_topology_specs[$idx]} -h $1 -o $remote_topology

        if [ "$?" == 0 ]; then
            echo "success!"
        else
            echo "failure!"
            /bin/rm -f $remote_topology
        fi
    done
}

run_test()
{
    # $1, 1st arg, is front-end program
    # $2, 2nd arg, is back-end program
    # $3, 3rd arg, says to use local or remote topology files
    # $4, 4th arg, specifies the shared object file, if applicable
    # $5, 5th arg, says to use standard or lightweight output file names
    front_end=$1
    back_end=$2
    test=`basename $front_end`

    for (( idx = 0 ; idx < ${#topologies[@]}; idx++ ));
    do
        topology=${topologies[$idx]}
        if [ $3 = "remote" ]; then
            topology_file="remote-$topology.top"
            if [ "$5" = "lightweight" ]; then
                outfile="$test-remote-lightweight-$topology.out"
                logfile="$test-remote-lightweight-$topology.log"
            else
                outfile="$test-remote-$topology.out"
                logfile="$test-remote-$topology.log"
            fi
            echo -n "Running $test(\"remote\", \"$topology\") ... "
        else
            topology_file="$topology_dir/local-$topology.top"
            if [ "$5" = "lightweight" ]; then
                outfile="$test-local-lightweight-$topology.out"
                logfile="$test-local-lightweight-$topology.log"
            else
                outfile="$test-local-$topology.out"
                logfile="$test-local-$topology.log"
            fi
            echo -n "Running $test(\"local\", \"$topology\") ... "
        fi

        if [ ! -f $topology_file ]; then
            echo "Error: topology file $topology_file does not exist."
        else

            /bin/rm -f $outfile
            /bin/rm -f $logfile

            case "$front_end" in
            "test_DynamicFilters_FE" )
                $front_end $4 $topology_file $back_end > $outfile 2> $logfile
                ;;
            "microbench_FE" )
                $front_end 5 500 $topology_file $back_end > $outfile 2> $logfile
                ;;
            "test_MultStreams_FE" )
                $front_end $topology_file 5 $back_end > $outfile 2> $logfile
                ;;
            * )
                $front_end $topology_file $back_end > $outfile 2> $logfile
                ;;
            esac
            if [ "$?" = 0 ]; then
                echo "exited with $?."
                echo -n "   Checking results ... ";
                /bin/rm -f mrnet_tests_grep.out
                grep -i failure $outfile > mrnet_tests_grep.out
                if [ "$?" != 0 ]; then
                    echo "success!"
                    /bin/rm -f $logfile
                else
                    echo "Error: (details in $outfile and $logfile)"
                fi
                /bin/rm -f mrnet_tests_grep.out
            else
                echo "Error: (details in $outfile and $logfile)"
            fi
        fi

    done
}

##### "main()" starts here
## Parse the command line

if [ $# -lt 1 ]; then
  print_usage
  exit -1;
fi

local="true"
remote="false"
sharedobject=""
lightweight="false"

while [ $# -gt 0 ]
do
  case "$1" in
    -lightweight )
        lightweight="true"
        shift
        ;;
    -l )
        local="true"
        shift
        ;;
    -a )
        local="true"
        remote="true"
        shift
        if [ $# -lt 1 ]; then
            print_usage
            echo "    Must specify a hostfile after -a option"
            exit -1;
        fi
        if test -r $1; then
            hostfile="$1"
        else
            echo "    $1 doesn't exist or is not readable"
            exit -1
        fi
        shift
        ;;
    -r )
        remote="true"
        local="false"
        shift
        if [ $# -lt 1 ]; then
            print_usage
            echo "    Must specify a hostfile after -r option"
            exit -1;
        fi
        if test -r "$1" ; then
            hostfile="$1"
        else
            echo "Error: $1 doesn't exist or is not readable"
            exit -1
        fi
		shift
        ;;
    -filter )
        sharedobject="${topology_dir}/test_DynamicFilters.so"
	if [ ! -f $sharedobject ]; then
	    sharedobject="{bin_dir}/../lib/test_DynamicFilters.so"
	    if [ ! -f $sharedobject ] ; then
                echo "Error: could not located test_DynamicFilters.so"
		exit
	    fi
	fi
        shift
        ;;
    *)
      print_usage;
      exit 0;
      ;;
  esac
  case $1 in 

  esac
done

if [ "$local" == "true" ]; then
    run_test "test_basic_FE" "test_basic_BE" "local" "" ""
    echo
    run_test "test_arrays_FE" "test_arrays_BE" "local" "" ""
    echo
    run_test "test_MultStreams_FE" "test_MultStreams_BE" "local" "" ""
    echo 
    run_test "test_NativeFilters_FE" "test_NativeFilters_BE" "local" "" "" 
    echo
    if [ "$sharedobject" != "" ]; then
        run_test "test_DynamicFilters_FE" "test_DynamicFilters_BE" "local" $sharedobject ""
        echo
    fi
    run_test "microbench_FE" "microbench_BE" "local" "" 
    echo
    if [ "$lightweight" == "true" ]; then
        run_test "test_basic_FE" "test_basic_BE_lightweight" "local" "" "lightweight" 
        echo
        run_test "test_arrays_FE" "test_arrays_BE_lightweight" "local" "" "lightweight" 
        echo
        run_test "test_MultStreams_FE" "test_MultStreams_BE_lightweight" "local" "" "lightweight" 
        echo
        run_test "test_NativeFilters_FE" "test_NativeFilters_BE_lightweight" "local" "" "lightweight"
        echo
        if [ "$sharedobject" != "" ]; then
            run_test "test_DynamicFilters_FE" "test_DynamicFilters_BE_lightweight" "local" $sharedobject "lightweight" 
            echo
        fi
        run_test "microbench_FE" "microbench_BE_lightweight" "local" "" "lightweight" 
        echo
    fi
fi

if [ "$remote" == "true" ]; then
    create_remote_topologies $hostfile

    run_test "test_basic_FE" "test_basic_BE" "remote" "" ""
    echo
    run_test "test_arrays_FE" "test_arrays_BE" "remote" "" ""
    echo
    run_test "test_MultStreams_FE" "test_MultStreams_BE" "remote" "" ""
    echo
    run_test "test_NativeFilters_FE" "test_NativeFilters_BE" "remote" "" ""
    echo
    if [ "$sharedobject" != "" ]; then
        run_test "test_DynamicFilters_FE" "test_DynamicFilters_BE" "remote" $sharedobject ""
        echo
    fi
    run_test "microbench_FE" "microbench_BE" "remote" "" ""
    echo
    if [ "$lightweight" == "true" ]; then
        run_test "test_basic_FE" "test_basic_BE_lightweight" "remote" "" "lightweight"
        echo
        run_test "test_arrays_FE" "test_arrays_BE_lightweight" "remote" "" "lightweight"
        echo
        run_test "test_MultStreams_FE" "test_MultStreams_BE_lightweight" "remote" "" "lightweight" 
        echo
        run_test "test_NativeFilters_FE" "test_NativeFilters_BE_lightweight" "remote" "" "lightweight" 
        echo
        if [ "$sharedobject" != "" ]; then
            run_test "test_DynamicFilters_FE" "test_DynamicFilters_BE_lightweight" "remote" $sharedobject "lightweight"
            echo
        fi
        run_test "microbench_FE" "microbench_BE_lightweight" "remote" "" "lightweight" 
        echo
    fi

fi
