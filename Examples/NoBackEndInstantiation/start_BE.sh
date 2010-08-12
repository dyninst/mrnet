#!/bin/sh

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


if [ $NBE -ne $NC ]; then
    echo Number of backends $NBE from $2 not equal to number of connections $NC from $3
    exit 1
fi

export ITER=0
while [ $ITER -lt $NBE ]; do
    # start BE on each host using parent info in BE_MAP
    CITER=`expr $ITER \* 4`
    chost=${BE_CONN[$CITER]}
    CITER=`expr $CITER + 1`
    cport=${BE_CONN[$CITER]}
    CITER=`expr $CITER + 1`
    crank=${BE_CONN[$CITER]}
    if [ "${BE_HOSTS[$ITER]}" = "localhost" -o "${BE_HOSTS[$ITER]}" = "$thishost" ] ; then
        $1 $chost $cport $crank ${BE_HOSTS[$ITER]} `expr $ITER + 10000` &
    else
        $REM_SHELL -n ${BE_HOSTS[$ITER]} $1 $chost $cport $crank ${BE_HOSTS[$ITER]} `expr $ITER + 10000` &
    fi 
    sleep 1
    ITER=`expr $ITER + 1`
done
