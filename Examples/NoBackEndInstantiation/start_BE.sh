#!/bin/bash

REM_SHELL=${XPLAT_RSH:-"ssh"}

if [ $# -ne 3 ]; then
    echo Usage: $0 be_executable be_host_list be_connection_list
    exit 1
fi

thishost=`hostname`

declare -a BE_HOSTS
export BE_HOSTS=( `cat $2` )
NBE=${#BE_HOSTS[*]}

declare -a BE_CONN
export BE_CONN=( `cat $3` )
NC=`expr ${#BE_CONN[*]} / 4`

export ITER=0
while [ $ITER -lt $NC ]; do
    # start BE on each host using parent info in BE_MAP
    CITER=`expr $ITER \* 4`
    chost=${BE_CONN[$CITER]}
    CITER=`expr $CITER + 1`
    cport=${BE_CONN[$CITER]}
    CITER=`expr $CITER + 1`
    crank=${BE_CONN[$CITER]}
    bendx=`expr $ITER \% $NBE`
    be=${BE_HOSTS[$bendx]}
    if [ "$be" = "localhost" -o "$be" = "$thishost" ] ; then
        $1 $chost $cport $crank $be `expr $ITER + 1000` &
    else
        $REM_SHELL -n $be $1 $chost $cport $crank $be `expr $ITER + 1000` &
    fi 
    sleep 1
    ITER=`expr $ITER + 1`
done
