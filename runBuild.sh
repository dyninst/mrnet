#!/bin/bash
#
# runBuild.sh - Build steps for mrnet
#
# 2021 Hewlett Packard Enterprise Development LP.
#
# Unpublished Proprietary Information.
# This unpublished work is protected to trade secret, copyright and other laws.
# Except as permitted by contract or express written permission of Hewlett Packard Enterprise Development LP.,
# no part of this work or its content may be used, reproduced or disclosed
# in any form.
#

echo "############################################"
echo "#            Setup environment.            #"
echo "############################################"

top_level=$PWD

source ./external/cdst_build_library/build_lib

setup_modules

module load cray-cdst-support
check_exit_status

module load cray-cti-devel
check_exit_status

module list

# Create install directory
source $top_level/release_versioning
mrnet_version=
branch_name=$(get_branch_info)
branch_type=$(echo "$branch_name" | cut -d'/' -f1)
if [ "$branch_type" != "release" ]; then
  mrnet_version="$major.$minor.$revision.$build_number"
else
  mrnet_version="$major.$minor.$revision"
fi
echo "mrnet version: \"$mrnet_version\""
install_dir="/opt/cray/pe/mrnet/$mrnet_version"
echo "Install dir: \"$install_dir\""
mkdir -p $install_dir
check_exit_status


echo "############################################"
echo "#              Building MRNet              #"
echo "############################################"

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

echo "cti_pkgconfig_L_dir:    $cti_pkgconfig_L_dir"
echo "cti_pkgconfig_I_dir:    $cti_pkgconfig_I_dir"
echo "cti_lib_directory:      $cti_lib_directory"
echo "cti_include_directory:  $cti_include_directory"

if [[ ! -d ${CRAY_CDST_SUPPORT_INSTALL_DIR} ]]; then
    echo "Could not find cray-cdst-support install path."
    exit 1
fi

mrnet_cflags=$(pkg-config --cflags cray-cdst-support)
mrnet_cxxflags=${mrnet_cflags}
mrnet_ldflags="$(pkg-config --libs cray-cdst-support) "'-Wl,-rpath,\$$ORIGIN'
mrnet_soflags="-Wl,-rpath,/opt/cray/pe/gcc-libs"
mrnet_ctiflags=$(pkg-config --libs common_tools_be)
mrnet_flags="${mrnet_ldflags} ${mrnet_ctiflags}"

echo "mrnet_cflags:     $mrnet_cflags"
echo "mrnet_cxxflags:   $mrnet_cxxflags"
echo "mrnet_ldflags:    $mrnet_ldflags"
echo "mrnet_soflags:    $mrnet_soflags"
echo "mrnet_ctiflags:   $mrnet_ctiflags"

CFLAGS=${mrnet_cflags} CXXFLAGS=${mrnet_cxxflags} LDFLAGS=${mrnet_flags} SOFLAGS=${mrnet_soflags} ./configure --prefix=$install_dir --with-startup=cray-cti --enable-shared --with-craycti-inc=${cti_include_directory} --with-craycti-lib=${cti_lib_directory} --with-boost=${CRAY_CDST_SUPPORT_INSTALL_DIR}
check_exit_status

# parallel builds are broken with mrnet
make
check_exit_status

make install
check_exit_status

# deliver extra config files
cp -a $top_level/build/*/xplat_config.h $install_dir/include/.
check_exit_status
cp -a $top_level/build/*/mrnet_config.h $install_dir/include/.
check_exit_status

echo "############################################"
echo "#              Done with build             #"
echo "############################################"

exit_with_status
