#!/bin/bash

if [ $# -ne 3 ]; then
  echo "ERROR: Usage - $0 target install-dir link-name"
  exit 1
fi

target=$1
idir=$2
lnk=$3

echo "Installing symlink to $target with name $lnk in directory $idir"

if [ ! -d $idir ]; then
  echo "ERROR: $0 - installation directory $idir not found"
  exit 1
fi

cd $idir

if [ ! -e $target ]; then
  echo "ERROR: $0 - target $idir/$target not found"
  exit 1
fi

if [ -e $lnk ]; then
  echo "- removing existing file $idir/$lnk"
  /bin/rm -f $lnk
fi

ln_cmd=`which ln`
if [ $? -eq 0 ]; then
  $ln_cmd -s $target $lnk
  result=$?
else
  echo "ERROR: $0 - failed to create symlink"
  result=1
fi

cd -

exit $result
