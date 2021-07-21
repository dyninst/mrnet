#!/bin/bash

# Build mrnet tests
PLATDIR=$PWD/build/x86_64-pc-linux-gnu
top_level=$PWD

module load cray-cdst-support
module load cray-cti-devel
module load cray-mrnet
module list

cti_pkgconfig_L_dir=$(pkg-config --libs-only-L common_tools_fe)
if [[ $? -ne 0 ]]; then
    echo "Could not find CTI in pkgconfig path. CTI may not be installed. Cannot continue with MRNet build."
    exit 1
fi

cti_pkgconfig_I_dir=$(pkg-config --cflags-only-I common_tools_fe)
if [[ $? -ne 0 ]]; then
    echo "Could not find CTI in pkgconfig path. CTI may not be installed. Cannot continue with MRNet build."
    exit 1
fi

cti_lib_directory=$(echo $cti_pkgconfig_L_dir | cut -c 3-)
cti_include_directory=$(echo $cti_pkgconfig_I_dir | cut -c 3-)

echo "cti_pkgconfig_L_dir:      $cti_pkgconfig_L_dir"
echo "cti_pkgconfig_I_dir:      $cti_pkgconfig_I_dir"
echo "cti_lib_directory:        $cti_lib_directory"
echo "cti_include_directory:    $cti_include_directory"

if [[ ! -d ${CRAY_CDST_SUPPORT_INSTALL_DIR} ]]; then
    echo "Could not find cray-cdst-support install path."
    exit 1
fi

# cflags
cdst_cflags=$(pkg-config --cflags cray-cdst-support)
mrnet_cflags=$(pkg-config --cflags cray-mrnet)    
total_cdst_cflags="-I$PWD/xplat/include ${cdst_cflags} ${mrnet_cflags}"
total_cdst_cxxflags=${total_cdst_cflags}

#ldflags
gcc_soflags="-Wl,-rpath,/opt/cray/pe/gcc-libs"
cdst_ldflags="$(pkg-config --libs cray-cdst-support) "'-Wl,-rpath,\$$ORIGIN'
cti_ldflags=$(pkg-config --libs common_tools_be)
mrnet_ldflags=$(pkg-config --libs cray-mrnet)
total_ldflags="${cdst_ldflags} ${cti_ldflags} ${mrnet_ldflags}"

echo "cdst_cflags:     $cdst_cflags"
echo "cdst_ldflags:    $cdst_ldflags"
echo "mrnet_cflags:    $mrnet_cflags"
echo "total_cdst_cflags:     $total_cdst_cflags"
echo "total_cdst_cxxflags:   $total_cdst_cxxflags"
echo
echo "gcc_soflags:     $gcc_soflags"
echo "cdst_ldflags:    $cdst_ldflags"
echo "cti_ldflags:     $cti_ldflags"
echo "mrnet_ldflags:   $mrnet_ldflags"
echo "total_ldflags:   $total_ldflags"
echo 

CFLAGS=${total_cdst_cflags} CXXFLAGS=${total_cdst_cxxflags} LDFLAGS=${total_ldflags} SOFLAGS=${gcc_soflags} ./configure --prefix=$PWD --with-startup=cray-cti --enable-shared --with-craycti-inc=${cti_include_directory} --with-craycti-lib=${cti_lib_directory} --with-boost=${CRAY_CDST_SUPPORT_INSTALL_DIR}

make -f $PLATDIR/Makefile tests

echo "Done"






