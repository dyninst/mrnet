# Packaging definitions
%global pkgversion %(%{_sourcedir}/get_package_data --crayversion)
%global branch %(%{_sourcedir}/get_package_data --branch)
%global pkgversion_separator -
%global copyright Copyright 2021 Hewlett Packard Enterprise Development LP.

# RPM build time
%global release_date %(date +%%B\\ %%Y)

%global cray_product_prefix cray-
%global product mrnet
%global cray_product %{product}
%global cray_name %{cray_product_prefix}%{product}

# Path definitions
%global cray_prefix /opt/cray/pe

#FIXME: This should be relocatable
%global external_build_dir %{cray_prefix}/%{cray_product}/%{pkgversion}

%global tests_source_dir %{_topdir}/../tests
%global demo_source_dir  %{_topdir}/../Examples
%global include_dir      %{_topdir}/../include
%global src_dir          %{_topdir}/../src
%global conf_dir         %{_topdir}/../conf
%global xplat_dir        %{_topdir}/../xplat

# pkgconfig template
%global pkgconfig_template    cray-mrnet.pc.in

# modulefile definitions
%global modulefile_name      %{cray_name}
%global module_template_name %{modulefile_name}.module.template

# set_default modulefile definitions
%global set_default_command       set_default
%global set_default_template      set_default_template
%global set_default_path          admin-pe/set_default_files

# lmod modulefiles
%global lmod_template_mrnet        template_%{cray_name}.lua

# release info
%global release_info_name          release_info
%global release_info_template_name %{release_info_name}.template

# yaml file
%global yaml_template yaml.template
%global removal_date  %(date '+%Y-%m-%d' -d "+5 years")

# attributions file
%global attributions_name ATTRIBUTIONS_mrnet.txt

# cdst-support version
%global cdst_support_pkgversion_min %(%{_sourcedir}/get_package_data --cdstversionmin)
%global cdst_support_pkgversion_max %(%{_sourcedir}/get_package_data --cdstversionmax)

# cti version
%global cti_pkgversion_min %(%{_sourcedir}/get_package_data --ctiversionmin)
%global cti_pkgversion_max %(%{_sourcedir}/get_package_data --ctiversionmax)

# Disable debug package
%global debug_package %{nil}
%global _build_id_links none

# System strip command may be too old, use current path
%global __strip strip

# Filter requires - these all come from the cdst-support rpm
# These should match what is excluded in the cdst-support rpm specfile!
%global privlibs             libboost.*
%global privlibs %{privlibs}|libmrnet
%global privlibs %{privlibs}|libbitset
%global privlibs %{privlibs}|libmi
%global privlibs %{privlibs}|libssh2
%global privlibs %{privlibs}|libarchive
%global privlibs %{privlibs}|libsymtabAPI
%global privlibs %{privlibs}|libpcontrol
%global privlibs %{privlibs}|libdynElf
%global privlibs %{privlibs}|libdynDwarf
%global privlibs %{privlibs}|libcommon
%global privlibs %{privlibs}|libxplat
%global privlibs %{privlibs}|libedit
%global privlibs %{privlibs}|libdw
%global privlibs %{privlibs}|libebl.*
%global privlibs %{privlibs}|libelf
%global privlibs %{privlibs}|libtbbmalloc*
%global privlibs %{privlibs}|libtbb.*
%global __requires_exclude ^(%{privlibs})\\.so*

# Dist tags for SuSE need to be manually set
%if 0%{?suse_version}
%if 0%{?sle_version} == 150100
%global dist .sles15sp1
%global OS_HW_TAG 7.0
%global OS_WB_TAG sles15sp1
%endif
%if 0%{?sle_version} == 150200
%global dist .sles15sp2
%global OS_HW_TAG 7.0
%global OS_WB_TAG sles15sp2
%endif
%if 0%{?sle_version} == 150300
%global dist .sles15sp3
%global OS_HW_TAG 7.0
%global OS_WB_TAG sles15sp3
%endif
%endif

%if %{_arch} == aarch64
%global SYS_HW_TAG AARCH64
%global SYS_WB_TAG AARCH64
%endif
%if %{_arch} == x86_64
%global SYS_HW_TAG HARDWARE
%global SYS_WB_TAG WHITEBOX
%endif
%if 0%{?rhel} == 8
%global OS_HW_TAG el8
%global OS_WB_TAG el8
%endif

Summary:    Cray mrnet 
Name:       %{cray_name}-%{pkgversion}
# BUILD_METADATA is set by Jenkins
Version:    %(echo ${BUILD_METADATA})
%if %{branch} == "release"
Release:    %(echo ${BUILD_NUMBER})%{dist}
%else
Release:    1%{dist}
%endif
Prefix:     %{cray_prefix}
License:    %{copyright}
Vendor:     Hewlett Packard Enterprise Development LP.
Group:      Development/Tools/Debuggers
Provides:   %{cray_name} = %{pkgversion}
Requires:   set_default_3, cray-cdst-support >= %{cdst_support_pkgversion_min}, cray-cdst-support < %{cdst_support_pkgversion_max}, cray-cti >= %{cti_pkgversion_min}, cray-cti < %{cti_pkgversion_max}
Source0:    %{module_template_name}
Source2:    %{release_info_template_name}
Source3:    %{attributions_name}
Source4:    %{lmod_template_mrnet}
Source5:    %{yaml_template}
Source6:    %{pkgconfig_template}

%description
Cray mrnet %{pkgversion}
MRNet is a customizable, high-performance software infrastructure for building scalable tools and applications.

%package -n %{cray_name}-tests-%{pkgversion}
Summary:    Cray mrnet parallel debugger tests
Group:      Development/Tools/Debuggers
Provides:   %{cray_name}-tests = %{pkgversion}
Requires:   %{cray_name} = %{pkgversion}
%description -n %{cray_name}-tests-%{pkgversion}
Tests for Cray mrnet parallel debugger

%prep
# Run q(uiet) with build directory name, c(reate) subdirectory, disable T(arball) unpacking
%setup -q -n %{name} -c -T
%build
# external build
%{__sed} 's|<RELEASE_DATE>|%{release_date}|g;s|<VERSION>|%{pkgversion}|g;s|<RELEASE>|%{version}-%{release}|g;s|<COPYRIGHT>|%{copyright}|g;s|<CRAY_NAME>|%{cray_name}|g;s|<CRAY_PREFIX>|%{cray_prefix}|g;s|<ARCH>|%{_target_cpu}|g' %{SOURCE2} > ${RPM_BUILD_DIR}/%{release_info_name}
%{__cp} -a %{SOURCE3} ${RPM_BUILD_DIR}/%{attributions_name}

%install
# copy files from external install
#
# Cray PE package root
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}

# Information files
%{__cp} -a ${RPM_BUILD_DIR}/%{release_info_name} ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/%{release_info_name}
%{__cp} -a ${RPM_BUILD_DIR}/%{attributions_name} ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/%{attributions_name}

# include headers
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/include
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/include/libi
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/include/mrnet
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/include/mrnet_lightweight
%{__cp} -a %{include_dir}/libi/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/include/libi
%{__cp} -a %{include_dir}/mrnet/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/include/mrnet
%{__cp} -a %{include_dir}/mrnet_lightweight/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/include/mrnet_lightweight

# Libraries
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/lib
%{__cp} -a %{external_build_dir}/lib/libmrnet.so* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/lib
%{__cp} -a %{external_build_dir}/lib/libxplat.so* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/lib

# libexec binaries
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/libexec
%{__cp} -a %{external_build_dir}/bin/mrnet_commnode ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/libexec
%{__cp} -a %{external_build_dir}/bin/mrnet_topgen ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/libexec

# modulefile
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/modulefiles/%{modulefile_name}
%{__sed} 's|<PREFIX>|%{external_build_dir}|g;s|<CRAY_NAME>|%{cray_name}|g;s|<VERSION>|%{pkgversion}|g' %{SOURCE0} > ${RPM_BUILD_ROOT}/%{prefix}/modulefiles/%{modulefile_name}/%{pkgversion}

# pkg-config
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/lib/pkgconfig
%{__sed} 's|<PREFIX>|%{external_build_dir}|g;s|<CRAY_NAME>|%{cray_name}|g;s|<VERSION>|%{pkgversion}|g' %{SOURCE6} > ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/lib/pkgconfig/%{cray_name}.pc
# demo/Example files
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/TestApp
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/FaultRecovery
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/HeterogeneousFilters
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/IntegerAddition
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/NoBackEndInstantiation
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/PerformanceData
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/PerThreadStreams

%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/Makefile ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/Makefile
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/xt_exception.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_exception.h
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/xt_protocol.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_protocol.h
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/xt_toolBE.cpp ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_toolBE.cpp
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/xt_toolFE.cpp ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_toolFE.cpp
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/xt_toolFilters.cpp ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_toolFilters.cpp
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/xt_util.cpp ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_util.cpp
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/xt_util.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_util.h

%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/TestApp/Makefile ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/TestApp/Makefile
%{__cp} -a %{demo_source_dir}/CrayXT_ToolHelper/TestApp/xt_testapp.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/TestApp/xt_testapp.c

%{__cp} -a %{demo_source_dir}/FaultRecovery/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/FaultRecovery
%{__cp} -a %{demo_source_dir}/HeterogeneousFilters/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/HeterogeneousFilters
%{__cp} -a %{demo_source_dir}/IntegerAddition/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/IntegerAddition
%{__cp} -a %{demo_source_dir}/NoBackEndInstantiation/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/NoBackEndInstantiation
%{__cp} -a %{demo_source_dir}/PerformanceData/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/PerformanceData
%{__cp} -a %{demo_source_dir}/PerThreadStreams/* ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/Examples/PerThreadStreams

# lmod modulefile
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/lmod/modulefiles/core/%{cray_name}
%{__sed} 's|\[@%PREFIX_PATH%@\]|%{prefix}|g;s|\[@%MODULE_VERSION%@\]|%{pkgversion}|g' %{SOURCE4} > ${RPM_BUILD_ROOT}/%{prefix}/lmod/modulefiles/core/%{cray_name}/%{pkgversion}.lua

# set_default template
%{__sed} 's|\[product_name\]|%{cray_product}|g;s|\[version_string\]|%{pkgversion}|g;s|\[install_dir\]|%{prefix}|g;s|\[module_dir\]|%{prefix}/modulefiles|g;s|\[module_name_list\]|%{modulefile_name}|g;s|\[lmod_dir_list\]|%{prefix}/lmod/modulefiles/core|g;s|\[lmod_name_list\]|%{modulefile_name}|g' %{_sourcedir}/set_default/%{set_default_template} > ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/%{set_default_command}_%{cray_name}_%{pkgversion}

# set_default into admin-pe
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{set_default_path}
%{__install} -D ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/%{set_default_command}_%{cray_name}_%{pkgversion} ${RPM_BUILD_ROOT}/%{prefix}/%{set_default_path}/%{set_default_command}_%{cray_name}_%{pkgversion}

# yaml file
%{__mkdir} -p %{_rpmdir}/%{_arch}
%global start_rmLine %(sed -n /section-3/= %{SOURCE5})
%global end_rmLine %(sed -n /admin-pe/= %{SOURCE5} | tail -1)
%if %{branch} == "release"
%{__sed} 's|<PRODUCT>|%{cray_name}|g;s|<VERSION>|%{pkgversion}|g;s|<BUILD_METADATA>|%{version}|g;s|<RELEASE>|%{release}|g;s|<ARCH>|%{_arch}|g;s|<REMOVAL_DATE>|%{removal_date}|g;s|<SYS_HW_TAG>|%{SYS_HW_TAG}|g;s|<SYS_WB_TAG>|%{SYS_WB_TAG}|g;s|<OS_HW_TAG>|%{OS_HW_TAG}|g;s|<OS_WB_TAG>|%{OS_WB_TAG}|g' %{SOURCE5} > %{_rpmdir}/%{_arch}/%{cray_name}-%{pkgversion}-%{version}-%{release}.%{_arch}.rpm.yaml
%else
%{__sed} '%{start_rmLine},%{end_rmLine}d;s|section-5|section-3|g;s|<PRODUCT>|%{cray_name}|g;s|<VERSION>|%{pkgversion}|g;s|<BUILD_METADATA>|%{version}|g;s|<RELEASE>|%{release}|g;s|<ARCH>|%{_arch}|g;s|<REMOVAL_DATE>|%{removal_date}|g;s|<SYS_HW_TAG>|%{SYS_HW_TAG}|g;s|<SYS_WB_TAG>|%{SYS_WB_TAG}|g;s|<OS_HW_TAG>|%{OS_HW_TAG}|g;s|<OS_WB_TAG>|%{OS_WB_TAG}|g' %{SOURCE5} > %{_rpmdir}/%{_arch}/%{cray_name}-%{pkgversion}-%{version}-%{release}.%{_arch}.rpm.yaml
%endif
# yaml file - tests (No modulefile provided)
%{__sed} '%{start_rmLine},%{end_rmLine}d;s|section-5|section-3|g;s|<PRODUCT>|%{cray_name}-tests|g;s|<VERSION>|%{pkgversion}|g;s|<BUILD_METADATA>|%{version}|g;s|<RELEASE>|%{release}|g;s|<ARCH>|%{_arch}|g;s|<REMOVAL_DATE>|%{removal_date}|g;s|<SYS_HW_TAG>|%{SYS_HW_TAG}|g;s|<SYS_WB_TAG>|%{SYS_WB_TAG}|g;s|<OS_HW_TAG>|%{OS_HW_TAG}|g;s|<OS_WB_TAG>|%{OS_WB_TAG}|g;' %{SOURCE5} > %{_rpmdir}/%{_arch}/%{cray_name}-tests-%{pkgversion}-%{version}-%{release}.%{_arch}.rpm.yaml

# tests
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files

%{__cp} -a %{tests_source_dir}/cray_launch/10_node_topo.def ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch/
%{__cp} -a %{tests_source_dir}/cray_launch/build_topo.py ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch/
%{__cp} -a %{tests_source_dir}/cray_launch/launch_test.ksh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch/

%{__cp} -a %{tests_source_dir}/test_Recovery/set.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/test_Recovery_aux.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/test_Recovery_aux.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/test_Recovery_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/test_Recovery_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/test_Recovery.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/test_Recovery.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/test_RecoveryFilter.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/ThroughputExperiment.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/ThroughputExperiment.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/
%{__cp} -a %{tests_source_dir}/test_Recovery/Timer.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/

%{__cp} -a %{tests_source_dir}/topology_files/local-1x1.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/
%{__cp} -a %{tests_source_dir}/topology_files/local-1x1x1x1.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/
%{__cp} -a %{tests_source_dir}/topology_files/local-1x1x2.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/
%{__cp} -a %{tests_source_dir}/topology_files/local-1x2x4.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/
%{__cp} -a %{tests_source_dir}/topology_files/local-1x4.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/
%{__cp} -a %{tests_source_dir}/topology_files/local-1x4x16.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/
%{__cp} -a %{tests_source_dir}/topology_files/local-1x16.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/

%{__cp} -a %{tests_source_dir}/config_generator.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/microbench_BE_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/microbench_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/microbench_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/microbench_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/microbench.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/mrnet_tests.sh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/ntKludges.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/singlecast_BE_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/singlecast_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/singlecast_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/singlecast_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/singlecast.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_arrays_BE_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_arrays_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_arrays_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_arrays_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_arrays.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_basic_BE_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_basic_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_basic_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_basic_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_basic.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_common.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_common.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_DynamicFilters_BE_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_DynamicFilters_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_DynamicFilters_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_DynamicFilters_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_DynamicFilters.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_DynamicFilters.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_MultStreams_BE_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_MultStreams_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_MultStreams_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_NativeFilters_BE_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_NativeFilters_BE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_NativeFilters_FE.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_NativeFilters_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/test_NativeFilters.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/timer.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/Topology.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/
%{__cp} -a %{tests_source_dir}/Topology.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/

# test conf files
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf
%{__cp} -a %{conf_dir}/aclocal.m4 ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/aclocal.m4
%{__cp} -a %{conf_dir}/check_configuration ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/check_configuration
%{__cp} -a %{conf_dir}/config.guess ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/config.guess
%{__cp} -a %{conf_dir}/config.h.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/config.h.in
%{__cp} -a %{conf_dir}/config.sub ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/config.sub
%{__cp} -a %{conf_dir}/configure.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/configure.in
%{__cp} -a %{conf_dir}/gen_lib_version_links.sh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/gen_lib_version_links.sh
%{__cp} -a %{conf_dir}/install_link.sh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/install_link.sh
%{__cp} -a %{conf_dir}/install-sh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/install-sh
%{__cp} -a %{conf_dir}/makedepends.sh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/makedepends.sh
%{__cp} -a %{conf_dir}/Makefile_tld.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile_tld.in
%{__cp} -a %{conf_dir}/Makefile.examples.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile.examples.in
%{__cp} -a %{conf_dir}/Makefile.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile.in
%{__cp} -a %{conf_dir}/Makefile.ltwt.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile.ltwt.in
%{__cp} -a %{_topdir}/../configure ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/configure
%{__cp} -a %{_topdir}/../build.sh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/build.sh
%{__cp} -a %{_topdir}/../Makefile ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/Makefile
%{__cp} -a %{_topdir}/../run_tests.sh ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/run_tests.sh
chmod a+rx -R ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/conf

# topology files
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files
%{__cp} -a %{tests_source_dir}/topology_files/local-1x1.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x1.top
%{__cp} -a %{tests_source_dir}/topology_files/local-1x1x1x1.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x1x1x1.top
%{__cp} -a %{tests_source_dir}/topology_files/local-1x1x2.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x1x2.top
%{__cp} -a %{tests_source_dir}/topology_files/local-1x2x4.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x2x4.top
%{__cp} -a %{tests_source_dir}/topology_files/local-1x4.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x4.top
%{__cp} -a %{tests_source_dir}/topology_files/local-1x4x16.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x4x16.top
%{__cp} -a %{tests_source_dir}/topology_files/local-1x16.top ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x16.top

# src files
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight
%{__cp} -a %{src_dir}/BackEndNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/BackEndNode.C
%{__cp} -a %{src_dir}/BackEndNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/BackEndNode.h
%{__cp} -a %{src_dir}/CTINetwork.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/CTINetwork.C
%{__cp} -a %{src_dir}/CTINetwork.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/CTINetwork.h
%{__cp} -a %{src_dir}/ChildNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ChildNode.C
%{__cp} -a %{src_dir}/ChildNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ChildNode.h
%{__cp} -a %{src_dir}/CommunicationNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/CommunicationNode.C
%{__cp} -a %{src_dir}/CommunicationNodeMain.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/CommunicationNodeMain.C
%{__cp} -a %{src_dir}/Communicator.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Communicator.C
%{__cp} -a %{src_dir}/DataElement.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/DataElement.C
%{__cp} -a %{src_dir}/Error.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Error.C
%{__cp} -a %{src_dir}/Event.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Event.C
%{__cp} -a %{src_dir}/EventDetector.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/EventDetector.C
%{__cp} -a %{src_dir}/EventDetector.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/EventDetector.h
%{__cp} -a %{src_dir}/FailureManagement.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/FailureManagement.C
%{__cp} -a %{src_dir}/FailureManagement.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/FailureManagement.h
%{__cp} -a %{src_dir}/Filter.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Filter.C
%{__cp} -a %{src_dir}/Filter.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Filter.h
%{__cp} -a %{src_dir}/FilterDefinitions.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/FilterDefinitions.C
%{__cp} -a %{src_dir}/FilterDefinitions.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/FilterDefinitions.h
%{__cp} -a %{src_dir}/FrontEndNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/FrontEndNode.C
%{__cp} -a %{src_dir}/FrontEndNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/FrontEndNode.h
%{__cp} -a %{src_dir}/InternalNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/InternalNode.C
%{__cp} -a %{src_dir}/InternalNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/InternalNode.h
%{__cp} -a %{src_dir}/LIBINetwork.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/LIBINetwork.C
%{__cp} -a %{src_dir}/LIBINetwork.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/LIBINetwork.h
%{__cp} -a %{src_dir}/Message.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Message.C
%{__cp} -a %{src_dir}/Message.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Message.h
%{__cp} -a %{src_dir}/NetworkTopology.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/NetworkTopology.C
%{__cp} -a %{src_dir}/Packet.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Packet.C
%{__cp} -a %{src_dir}/ParadynFilterDefinitions.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParadynFilterDefinitions.C
%{__cp} -a %{src_dir}/ParadynFilterDefinitions.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParadynFilterDefinitions.h
%{__cp} -a %{src_dir}/ParentNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParentNode.C
%{__cp} -a %{src_dir}/ParentNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParentNode.h
%{__cp} -a %{src_dir}/ParsedGraph.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParsedGraph.C
%{__cp} -a %{src_dir}/ParsedGraph.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParsedGraph.h
%{__cp} -a %{src_dir}/PeerNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/PeerNode.C
%{__cp} -a %{src_dir}/PeerNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/PeerNode.h
%{__cp} -a %{src_dir}/PerfDataEvent.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataEvent.C
%{__cp} -a %{src_dir}/PerfDataEvent.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataEvent.h
%{__cp} -a %{src_dir}/PerfDataSysEvent_linux.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataSysEvent_linux.C
%{__cp} -a %{src_dir}/PerfDataSysEvent_none.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataSysEvent_none.C
%{__cp} -a %{src_dir}/PerfDataSysEvent.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataSysEvent.h
%{__cp} -a %{src_dir}/Protocol.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Protocol.h
%{__cp} -a %{src_dir}/RSHBackEndNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHBackEndNode.C
%{__cp} -a %{src_dir}/RSHBackEndNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHBackEndNode.h
%{__cp} -a %{src_dir}/RSHChildNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHChildNode.C
%{__cp} -a %{src_dir}/RSHChildNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHChildNode.h
%{__cp} -a %{src_dir}/RSHFrontEndNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHFrontEndNode.C
%{__cp} -a %{src_dir}/RSHFrontEndNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHFrontEndNode.h
%{__cp} -a %{src_dir}/RSHInternalNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHInternalNode.C
%{__cp} -a %{src_dir}/RSHInternalNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHInternalNode.h
%{__cp} -a %{src_dir}/RSHNetwork.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHNetwork.C
%{__cp} -a %{src_dir}/RSHNetwork.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHNetwork.h
%{__cp} -a %{src_dir}/RSHParentNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHParentNode.C
%{__cp} -a %{src_dir}/RSHParentNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHParentNode.h
%{__cp} -a %{src_dir}/Router.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Router.C
%{__cp} -a %{src_dir}/Router.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Router.h
%{__cp} -a %{src_dir}/SerialGraph.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/SerialGraph.C
%{__cp} -a %{src_dir}/SerialGraph.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/SerialGraph.h
%{__cp} -a %{src_dir}/Stream.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Stream.C
%{__cp} -a %{src_dir}/TimeKeeper.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/TimeKeeper.C
%{__cp} -a %{src_dir}/TimeKeeper.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/TimeKeeper.h
%{__cp} -a %{src_dir}/Tree.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/Tree.C
%{__cp} -a %{src_dir}/XTBackEndNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTBackEndNode.C
%{__cp} -a %{src_dir}/XTBackEndNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTBackEndNode.h
%{__cp} -a %{src_dir}/XTFrontEndNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTFrontEndNode.C
%{__cp} -a %{src_dir}/XTFrontEndNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTFrontEndNode.h
%{__cp} -a %{src_dir}/XTInternalNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTInternalNode.C
%{__cp} -a %{src_dir}/XTInternalNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTInternalNode.h
%{__cp} -a %{src_dir}/XTNetwork.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTNetwork.C
%{__cp} -a %{src_dir}/XTNetwork.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTNetwork.h
%{__cp} -a %{src_dir}/XTParentNode.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTParentNode.C
%{__cp} -a %{src_dir}/XTParentNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTParentNode.h
%{__cp} -a %{src_dir}/byte_order.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/byte_order.h
%{__cp} -a %{src_dir}/parser.y ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/parser.y
%{__cp} -a %{src_dir}/parser.tab.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/parser.tab.C
%{__cp} -a %{src_dir}/parser.tab.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/parser.tab.h
%{__cp} -a %{src_dir}/pdr.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/pdr.c
%{__cp} -a %{src_dir}/pdr.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/pdr.h
%{__cp} -a %{src_dir}/scanner.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/scanner.C
%{__cp} -a %{src_dir}/scanner.l ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/scanner.l
%{__cp} -a %{src_dir}/utils.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/utils.C
%{__cp} -a %{src_dir}/utils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/utils.h


%{__cp} -a %{src_dir}/lightweight/BackEndNode.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/BackEndNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/ChildNode.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/ChildNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/DataElement.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Error.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Filter.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Filter.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/FilterDefinitions.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/FilterDefinitions.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Message.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Message.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Network.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Network.c
%{__cp} -a %{src_dir}/lightweight/NetworkTopology.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Packet.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PeerNode.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PeerNode.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PerfDataEvent.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PerfDataEvent.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PerfDataSysEvent_linux.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PerfDataSysEvent_none.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PerfDataSysEvent_win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/PerfDataSysEvent.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/SerialGraph.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/SerialGraph.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/Stream.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/utils_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/
%{__cp} -a %{src_dir}/lightweight/utils_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/

# xplat files
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf
%{__cp} -a %{xplat_dir}/conf/aclocal.m4 ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/aclocal.m4
%{__cp} -a %{xplat_dir}/conf/config.h.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/config.h.in
%{__cp} -a %{xplat_dir}/conf/Makefile.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/Makefile.in
%{__cp} -a %{xplat_dir}/conf/Makefile.ltwt.in ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/Makefile.ltwt.in

%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include
%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat
%{__cp} -a %{xplat_dir}/include/xplat/Atomic.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Atomic.h
%{__cp} -a %{xplat_dir}/include/xplat/Error.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Error.h
%{__cp} -a %{xplat_dir}/include/xplat/Mutex.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Mutex.h
%{__cp} -a %{xplat_dir}/include/xplat/Monitor.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Monitor.h
%{__cp} -a %{xplat_dir}/include/xplat/NetUtils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/NetUtils.h
%{__cp} -a %{xplat_dir}/include/xplat/Once.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Once.h
%{__cp} -a %{xplat_dir}/include/xplat/PathUtils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/PathUtils.h
%{__cp} -a %{xplat_dir}/include/xplat/Process.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Process.h
%{__cp} -a %{xplat_dir}/include/xplat/SharedObject.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/SharedObject.h
%{__cp} -a %{xplat_dir}/include/xplat/SocketUtils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/SocketUtils.h
%{__cp} -a %{xplat_dir}/include/xplat/Thread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Thread.h
%{__cp} -a %{xplat_dir}/include/xplat/TLSKey.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/TLSKey.h
%{__cp} -a %{xplat_dir}/include/xplat/Tokenizer.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Tokenizer.h
%{__cp} -a %{xplat_dir}/include/xplat/Types-unix.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Types-unix.h
%{__cp} -a %{xplat_dir}/include/xplat/Types-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Types-win.h
%{__cp} -a %{xplat_dir}/include/xplat/Types.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Types.h
%{__cp} -a %{xplat_dir}/include/xplat/xplat_utils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/xplat_utils.h


%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Error.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Error.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/map.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/map.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Monitor.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Monitor.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Mutex.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Mutex.h 
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/NetUtils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/NetUtils.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/PathUtils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/PathUtils.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Process.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Process.h 
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/SocketUtils.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/SocketUtils.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Thread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Thread.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Types-unix.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Types-unix.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Types-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Types-win.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/Types.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Types.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/vector.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/vector.h
%{__cp} -a %{xplat_dir}/include/xplat_lightweight/xplat_utils_lightweight.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/xplat_utils_lightweight.h

%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src
%{__cp} -a %{xplat_dir}/src/Error-unix.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Error-unix.C
%{__cp} -a %{xplat_dir}/src/Error-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Error-win.C 
%{__cp} -a %{xplat_dir}/src/Monitor-pthread.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-pthread.C
%{__cp} -a %{xplat_dir}/src/Monitor-pthread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-pthread.h
%{__cp} -a %{xplat_dir}/src/Monitor-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-win.C
%{__cp} -a %{xplat_dir}/src/Monitor-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-win.h
%{__cp} -a %{xplat_dir}/src/Mutex-pthread.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Mutex-pthread.C
%{__cp} -a %{xplat_dir}/src/Mutex-pthread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Mutex-pthread.h 
%{__cp} -a %{xplat_dir}/src/NetUtils-unix.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/NetUtils-unix.C
%{__cp} -a %{xplat_dir}/src/Once-pthread.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-pthread.C
%{__cp} -a %{xplat_dir}/src/Once-pthread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-pthread.h
%{__cp} -a %{xplat_dir}/src/Once-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-win.C
%{__cp} -a %{xplat_dir}/src/Once-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-win.h
%{__cp} -a %{xplat_dir}/src/PathUtils-unix.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/PathUtils-unix.C
%{__cp} -a %{xplat_dir}/src/PathUtils-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/PathUtils-win.C
%{__cp} -a %{xplat_dir}/src/Process-unix.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Process-unix.C 
%{__cp} -a %{xplat_dir}/src/Process-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Process-win.C 
%{__cp} -a %{xplat_dir}/src/Process.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Process.C
%{__cp} -a %{xplat_dir}/src/SharedObject-unix.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-unix.C
%{__cp} -a %{xplat_dir}/src/SharedObject-unix.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-unix.h
%{__cp} -a %{xplat_dir}/src/SharedObject-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-win.C 
%{__cp} -a %{xplat_dir}/src/SharedObject-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-win.h
%{__cp} -a %{xplat_dir}/src/SocketUtils-unix.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SocketUtils-unix.C 
%{__cp} -a %{xplat_dir}/src/SocketUtils-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SocketUtils-win.C 
%{__cp} -a %{xplat_dir}/src/Thread-pthread.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Thread-pthread.C 
%{__cp} -a %{xplat_dir}/src/Thread-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Thread-win.C
%{__cp} -a %{xplat_dir}/src/TLSKey-pthread.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-pthread.C 
%{__cp} -a %{xplat_dir}/src/TLSKey-pthread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-pthread.h
%{__cp} -a %{xplat_dir}/src/TLSKey-win.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-win.C
%{__cp} -a %{xplat_dir}/src/TLSKey-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-win.h
%{__cp} -a %{xplat_dir}/src/Tokenizer.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Tokenizer.C 
%{__cp} -a %{xplat_dir}/src/xplat_utils.C ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/xplat_utils.C 

%{__install} -d ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight
%{__cp} -a %{xplat_dir}/src/lightweight/Error-unix.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Error-unix.c
%{__cp} -a %{xplat_dir}/src/lightweight/Error-win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Error-win.c
%{__cp} -a %{xplat_dir}/src/lightweight/map.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/map.c 
%{__cp} -a %{xplat_dir}/src/lightweight/Monitor-pthread.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-pthread.c
%{__cp} -a %{xplat_dir}/src/lightweight/Monitor-pthread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-pthread.h
%{__cp} -a %{xplat_dir}/src/lightweight/Monitor-win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-win.c 
%{__cp} -a %{xplat_dir}/src/lightweight/Monitor-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-win.h 
%{__cp} -a %{xplat_dir}/src/lightweight/Mutex-pthread.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-pthread.c 
%{__cp} -a %{xplat_dir}/src/lightweight/Mutex-pthread.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-pthread.h
%{__cp} -a %{xplat_dir}/src/lightweight/Mutex-win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-win.c
%{__cp} -a %{xplat_dir}/src/lightweight/Mutex-win.h ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-win.h
%{__cp} -a %{xplat_dir}/src/lightweight/NetUtils-unix.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/NetUtils-unix.c 
%{__cp} -a %{xplat_dir}/src/lightweight/NetUtils-win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/NetUtils-win.c
%{__cp} -a %{xplat_dir}/src/lightweight/PathUtils-unix.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/NetUtils-win.c
%{__cp} -a %{xplat_dir}/src/lightweight/PathUtils-win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/PathUtils-unix.c
%{__cp} -a %{xplat_dir}/src/lightweight/Process-unix.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Process-unix.c
%{__cp} -a %{xplat_dir}/src/lightweight/Process-win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Process-win.c 
%{__cp} -a %{xplat_dir}/src/lightweight/SocketUtils-unix.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/SocketUtils-unix.c
%{__cp} -a %{xplat_dir}/src/lightweight/SocketUtils-win.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/SocketUtils-win.c 
%{__cp} -a %{xplat_dir}/src/lightweight/SocketUtils.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/SocketUtils.c
%{__cp} -a %{xplat_dir}/src/lightweight/Thread-pthread.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Thread-pthread.c
%{__cp} -a %{xplat_dir}/src/lightweight/vector.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/vector.c
%{__cp} -a %{xplat_dir}/src/lightweight/xplat_utils_lightweight.c ${RPM_BUILD_ROOT}/%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/xplat_utils_lightweight.c 


%post
# Add Shasta configuration files for PE content projection
if [ "${RPM_INSTALL_PREFIX}" = "%{cray_prefix}" ]; then
    %{__mkdir_p} /etc/%{cray_prefix}/admin-pe/bindmount.conf.d/
    %{__mkdir_p} /etc/%{cray_prefix}/admin-pe/modulepaths.conf.d/
    echo "%{cray_prefix}/%{product}/" > /etc/%{cray_prefix}/admin-pe/bindmount.conf.d/%{cray_name}.conf
    echo "%{cray_prefix}/modulefiles/%{modulefile_name}" > /etc/%{cray_prefix}/admin-pe/modulepaths.conf.d/%{modulefile_name}.conf
    echo "%{cray_prefix}/lmod/modulefiles/core/%{cray_name}" >> /etc/%{cray_prefix}/admin-pe/modulepaths.conf.d/%{modulefile_name}.conf
fi

# Process relocations
if [ "${RPM_INSTALL_PREFIX}" != "%{prefix}" ]
then
    # Modulefile
    %{__sed} -i "s|^\([[:space:]]*set MRNET_BASEDIR[[:space:]]*\)\(.*\)|\1${RPM_INSTALL_PREFIX}/%{cray_product}/%{pkgversion}|" ${RPM_INSTALL_PREFIX}/modulefiles/%{modulefile_name}/%{pkgversion}
    # set default command
    %{__sed} -i "s|^\(export CRAY_inst_dir=\).*|\1${RPM_INSTALL_PREFIX}|" ${RPM_INSTALL_PREFIX}/%{cray_product}/%{pkgversion}/%{set_default_command}_%{cray_name}_%{pkgversion}

    # update the pkgconfig file to point to relocated install dir - prefix=<PREFIX>
   %{__sed} -i "s|^\(prefix=\).*|\1${RPM_INSTALL_PREFIX}/%{cray_product}/%{pkgversion}|" ${RPM_INSTALL_PREFIX}/%{cray_product}/%{pkgversion}/lib/pkgconfig/%{cray_name}.pc
else
    # Only call set_default if we are not relocating the rpm and there's not already a default set unless someone passes in CRAY_INSTALL_DEFAULT=1
    if [ ${CRAY_INSTALL_DEFAULT:-0} -eq 1 ] || [ ! -f ${RPM_INSTALL_PREFIX}/modulefiles/%{cray_name}/.version ]
    then
        ${RPM_INSTALL_PREFIX}/%{set_default_path}/%{set_default_command}_%{cray_name}_%{pkgversion}
    fi

    # Don't want to set LD_LIBRARY_PATH or MRNET_INSTALL_DIR if we are not relocating since rpath was set properly
    # tcl module
    %{__sed} -i "/^[[:space:]]*prepend-path[[:space:]]*LD_LIBRARY_PATH.*/d" ${RPM_INSTALL_PREFIX}/modulefiles/%{modulefile_name}/%{pkgversion}
    %{__sed} -i "/^[[:space:]]*setenv[[:space:]]*MRNET_INSTALL_DIR.*/d" ${RPM_INSTALL_PREFIX}/modulefiles/%{modulefile_name}/%{pkgversion}
    # lua module
    %{__sed} -i "/^prepend-path[[:space:]]*LD_LIBRARY_PATH.*/d" ${RPM_INSTALL_PREFIX}/lmod/modulefiles/core/%{cray_name}/%{pkgversion}.lua
    %{__sed} -i "/^setenv[[:space:]]*MRNET_INSTALL_DIR.*/d" ${RPM_INSTALL_PREFIX}/lmod/modulefiles/core/%{cray_name}/%{pkgversion}.lua
fi

%preun
default_link="${RPM_INSTALL_PREFIX}/%{cray_product}/default"

# Cleanup module .version if it points to this version
if [ -f ${RPM_INSTALL_PREFIX}/modulefiles/%{modulefile_name}/.version ]
then
  dotversion=`grep ModulesVersion ${RPM_INSTALL_PREFIX}/modulefiles/%{modulefile_name}/.version | cut -f2 -d'"'`

  if [ "$dotversion" == "%{pkgversion}" ]
  then
    %{__rm} -f ${RPM_INSTALL_PREFIX}/modulefiles/%{modulefile_name}/.version
    echo "Uninstalled version and .version file match = ${default_version}."
    echo "Removing %{modulefile_name} .version file."
    %{__rm} -f ${default_link}
  fi
fi

%postun
if [ $1 == 1 ]
then
  exit 0
fi

# If the install dir exists
if [[ -z `ls ${RPM_INSTALL_PREFIX}/%{cray_product}` ]]; then
  %{__rm} -rf ${RPM_INSTALL_PREFIX}/%{cray_product}
  if [ -f /etc%{prefix}/admin-pe/bindmount.conf.d/%{cray_name}.conf ]; then
    %{__rm} -rf /etc%{prefix}/admin-pe/bindmount.conf.d/%{cray_name}.conf
  fi
  if [ -f /etc%{prefix}/admin-pe/modulepaths.conf.d/%{cray_name}.conf ]; then
    %{__rm} -rf /etc%{prefix}/admin-pe/modulepaths.conf.d/%{cray_name}.conf
  fi
  if [ -d ${RPM_INSTALL_PREFIX}/lmod/modulefiles/core/%{cray_name} ]; then
    %{__rm} -rf ${RPM_INSTALL_PREFIX}/lmod/modulefiles/core/%{cray_name}
  fi
  if [ -d ${RPM_INSTALL_PREFIX}/modulefiles/%{cray_name} ]; then
    %{__rm} -rf ${RPM_INSTALL_PREFIX}/modulefiles/%{cray_name}
  fi
fi

%files
%defattr(755, root, root)
%dir %{prefix}/%{cray_product}/%{pkgversion}
%dir %{prefix}/%{cray_product}/%{pkgversion}/include
%dir %{prefix}/%{cray_product}/%{pkgversion}/lib
%dir %{prefix}/%{cray_product}/%{pkgversion}/lib/pkgconfig
%dir %{prefix}/%{cray_product}/%{pkgversion}/libexec
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/TestApp
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/FaultRecovery
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/HeterogeneousFilters
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/IntegerAddition
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/NoBackEndInstantiation
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/PerformanceData
%dir %{prefix}/%{cray_product}/%{pkgversion}/Examples/PerThreadStreams

%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/Makefile
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_exception.h
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_protocol.h
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_toolBE.cpp
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_toolFE.cpp
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_toolFilters.cpp
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_util.cpp
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/xt_util.h

%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/TestApp/Makefile
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/CrayXT_ToolHelper/TestApp/xt_testapp.c

%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/FaultRecovery/*
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/HeterogeneousFilters/*
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/IntegerAddition/*
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/NoBackEndInstantiation/*
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/PerformanceData/*
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/Examples/PerThreadStreams/*

%dir %{prefix}/%{cray_product}/%{pkgversion}/include/libi
%dir %{prefix}/%{cray_product}/%{pkgversion}/include/mrnet
%dir %{prefix}/%{cray_product}/%{pkgversion}/include/mrnet_lightweight
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/include/libi/*
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/include/mrnet/*
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/include/mrnet_lightweight/*

%{prefix}/%{cray_product}/%{pkgversion}/lib/libmrnet.so*
%{prefix}/%{cray_product}/%{pkgversion}/lib/libxplat.so*
%{prefix}/%{cray_product}/%{pkgversion}/libexec/mrnet_topgen
%{prefix}/%{cray_product}/%{pkgversion}/libexec/mrnet_commnode

%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/%{release_info_name}
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/%{attributions_name}
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/%{set_default_command}_%{cray_name}_%{pkgversion}
%attr(755, root, root) %{prefix}/%{set_default_path}/%{set_default_command}_%{cray_name}_%{pkgversion}
%attr(755, root, root) %{prefix}/modulefiles/%{modulefile_name}/%{pkgversion}
%attr(644, root, root) %{prefix}/lmod/modulefiles/core/%{cray_name}/%{pkgversion}.lua
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/lib/pkgconfig/cray-mrnet.pc

%files -n %{cray_name}-tests-%{pkgversion}
%dir %{prefix}/%{cray_product}/%{pkgversion}/tests
%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch
%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery

%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch/10_node_topo.def
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch/build_topo.py
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/cray_launch/launch_test.ksh

%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/set.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/test_Recovery_aux.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/test_Recovery_aux.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/test_Recovery_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/test_Recovery_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/test_Recovery.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/test_Recovery.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/test_RecoveryFilter.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/ThroughputExperiment.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/ThroughputExperiment.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_Recovery/Timer.h

%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/config_generator.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/microbench_BE_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/microbench_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/microbench_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/microbench_lightweight.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/microbench.h
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/mrnet_tests.sh
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/ntKludges.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/singlecast_BE_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/singlecast_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/singlecast_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/singlecast_lightweight.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/singlecast.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_arrays_BE_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_arrays_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_arrays_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_arrays_lightweight.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_arrays.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_basic_BE_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_basic_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_basic_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_basic_lightweight.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_basic.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_common.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_common.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_DynamicFilters_BE_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_DynamicFilters_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_DynamicFilters_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_DynamicFilters_lightweight.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_DynamicFilters.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_DynamicFilters.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_MultStreams_BE_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_MultStreams_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_MultStreams_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_NativeFilters_BE_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_NativeFilters_BE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_NativeFilters_FE.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_NativeFilters_lightweight.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/test_NativeFilters.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/timer.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/Topology.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/Topology.h

%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/conf
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/aclocal.m4
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/check_configuration
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/config.guess
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/config.h.in
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/config.sub
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/configure.in
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/gen_lib_version_links.sh
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/install_link.sh
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/install-sh
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/makedepends.sh
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile_tld.in
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile.examples.in
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile.in
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/conf/Makefile.ltwt.in
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/configure
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/build.sh
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/Makefile
%attr(755, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/run_tests.sh

%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x1.top
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x1x1x1.top
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x1x2.top
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x2x4.top
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x4.top
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x4x16.top
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/topology_files/local-1x16.top 

%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/src
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/BackEndNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/BackEndNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/CTINetwork.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/CTINetwork.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ChildNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ChildNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/CommunicationNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/CommunicationNodeMain.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Communicator.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/DataElement.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Error.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Event.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/EventDetector.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/EventDetector.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/FailureManagement.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/FailureManagement.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Filter.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Filter.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/FilterDefinitions.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/FilterDefinitions.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/FrontEndNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/FrontEndNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/InternalNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/InternalNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/LIBINetwork.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/LIBINetwork.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Message.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Message.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/NetworkTopology.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Packet.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParadynFilterDefinitions.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParadynFilterDefinitions.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParentNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParentNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParsedGraph.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/ParsedGraph.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/PeerNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/PeerNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataEvent.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataEvent.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataSysEvent_linux.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataSysEvent_none.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/PerfDataSysEvent.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Protocol.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHBackEndNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHBackEndNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHChildNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHChildNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHFrontEndNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHFrontEndNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHInternalNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHInternalNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHNetwork.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHNetwork.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHParentNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/RSHParentNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Router.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Router.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/SerialGraph.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/SerialGraph.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Stream.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/TimeKeeper.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/TimeKeeper.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/Tree.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTBackEndNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTBackEndNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTFrontEndNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTFrontEndNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTInternalNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTInternalNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTNetwork.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTNetwork.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTParentNode.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/XTParentNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/byte_order.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/parser.tab.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/parser.tab.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/pdr.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/scanner.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/utils.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/utils.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/pdr.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/parser.y
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/scanner.l

%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/BackEndNode.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/BackEndNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/ChildNode.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/ChildNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/DataElement.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Error.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Filter.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Filter.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/FilterDefinitions.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/FilterDefinitions.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Message.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Message.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Network.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/NetworkTopology.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Packet.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PeerNode.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PeerNode.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PerfDataEvent.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PerfDataEvent.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PerfDataSysEvent_linux.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PerfDataSysEvent_none.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PerfDataSysEvent_win.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/PerfDataSysEvent.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/SerialGraph.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/SerialGraph.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/Stream.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/utils_lightweight.c
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/src/lightweight/utils_lightweight.h


%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat
%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/aclocal.m4
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/config.h.in
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/Makefile.in
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/conf/Makefile.ltwt.in

%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include
%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Atomic.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Error.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Monitor.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Mutex.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/NetUtils.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Once.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/PathUtils.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Process.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/SharedObject.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/SocketUtils.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Thread.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/TLSKey.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Tokenizer.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Types-unix.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Types-win.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/Types.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat/xplat_utils.h


%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Error.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/map.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Monitor.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Mutex.h 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/NetUtils.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/PathUtils.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Process.h 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/SocketUtils.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Thread.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Types-unix.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Types-win.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/Types.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/vector.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/include/xplat_lightweight/xplat_utils_lightweight.h

%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Error-unix.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Error-win.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-pthread.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-pthread.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-win.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Monitor-win.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Mutex-pthread.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Mutex-pthread.h 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/NetUtils-unix.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-pthread.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-pthread.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-win.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Once-win.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/PathUtils-unix.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/PathUtils-win.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Process-unix.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Process-win.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Process.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-unix.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-unix.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-win.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SharedObject-win.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SocketUtils-unix.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/SocketUtils-win.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Thread-pthread.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Thread-win.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-pthread.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-pthread.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-win.C
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/TLSKey-win.h
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/Tokenizer.C 
%attr(644, root, root) %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/xplat_utils.C 

%dir %{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Error-unix.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Error-win.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/map.c 
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-pthread.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-pthread.h
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-win.c 
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Monitor-win.h 
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-pthread.c 
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-pthread.h
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-win.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Mutex-win.h
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/NetUtils-unix.c 
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/NetUtils-win.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/PathUtils-unix.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Process-unix.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Process-win.c 
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/SocketUtils-unix.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/SocketUtils-win.c 
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/SocketUtils.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/Thread-pthread.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/vector.c
%{prefix}/%{cray_product}/%{pkgversion}/tests/xplat/src/lightweight/xplat_utils_lightweight.c 