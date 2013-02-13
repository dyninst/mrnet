#!/bin/sh

if [ $# -ne 5 ]; then
    echo "ERROR: Usage - $0 libdir libname major minor revision"
    exit 1
fi

script_dir=`dirname $0`

libdir=$1
libname=$2
vmajor=$3
vminor=$4
vrev=$5

# link .so
orig_libso="${libname}.so.${vmajor}.${vminor}.${vrev}"
major_libso="${libname}.so.${vmajor}"
versioned_libso="${libname}.${vmajor}.${vminor}.${vrev}.so"
libso=${libname}.so
if [ -e $libdir/$orig_libso ]; then
    ${script_dir}/install_link.sh $orig_libso $libdir $major_libso
    ${script_dir}/install_link.sh $major_libso $libdir $libso
    ${script_dir}/install_link.sh $orig_libso $libdir $versioned_libso
fi


# link .a
orig_liba="${libname}.a.${vmajor}.${vminor}.${vrev}"
liba=${libname}.a
if [ -e $libdir/$orig_liba ]; then
    ${script_dir}/install_link.sh $orig_liba $libdir $liba
fi

