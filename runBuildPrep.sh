#!/bin/bash
#
# runBuildPrep.sh - Preps the build environment
#
# Copyright 2021 Hewlett Packard Enterprise Development LP.
#
# Unpublished Proprietary Information.
# This unpublished work is protected to trade secret, copyright and other laws.
# Except as permitted by contract or express written permission of Hewlett Packard Enterprise Development LP.,
# no part of this work or its content may be used, reproduced or disclosed
# in any form.
#

top_level=$PWD
source ./external/cdst_build_library/build_lib

echo "############################################"
echo "#             Installing deps              #"
echo "############################################"
target_pm=$(get_pm)
target_os=$(get_os)
if [[ "$target_pm" == "$cdst_pm_zypper" ]]; then
    # Install zypper based dependencies
    zypper --non-interactive install \
            autoconf \
            automake \
            binutils-devel \
            bison \
            bzip2 \
            ctags \
            cmake \
            flex \
            libtool \
            m4 \
            make \
            makeinfo \
            mksh \
            libbz2-devel \
            liblzma5 \
            libtool \
            rpm-build \
            tcl \
            which \
            xz-devel \
            python3-pip
    check_exit_status
elif [[ "$target_pm" == "$cdst_pm_yum" ]]; then
    if [[ "$target_os" == "$cdst_os_centos8" ]]; then
      # Note the following will be different on build VMs vs DST. Errors are okay.
      yum config-manager --set-enabled powertools
    else
      yum --assumeyes install subscription-manager
      subscription-manager repos --enable codeready-builder-for-rhel-8-x86_64-rpms
    fi
    # Install yum based components
    yum --assumeyes install \
            texinfo \
            autoconf \
            automake \
            binutils-devel \
            bison \
            bzip2 \
            bzip2-devel \
            ctags \
            cmake \
            environment-modules \
            flex \
            libtool \
            m4 \
            make \
            mksh \
            python3-devel \
            rpm-build \
            tcl \
            which \
            xz-devel
    check_exit_status
else
    # Unknown OS! Exit with error.
    echo "Unsupported Package Manager detected!"
    exit 1
fi


# Install the common PE components
install_common_pe

# Install CTI and CDST support
install_cti

capture_jenkins_build
check_exit_status

echo "############################################"
echo "#          Done with build prep            #"
echo "############################################"

exit_with_status
