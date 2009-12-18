#!/bin/sh

if [ $# -lt 1 ]; then
  echo "Usage: check_hosts.sh $hostfile"
  exit -1;
fi

hosts=`cat $1`

for host in $hosts
do
	echo -n "Checking processes on $host ..."

	ssh $host ps aux | grep $USER

	if [ "$?" != 0 ]; then
        echo "failure!"
    else
        echo "success!"
    fi
done
