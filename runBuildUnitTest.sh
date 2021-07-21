#
# Unpublished Proprietary Information.
# This unpublished work is protected to trade secret, copyright and other laws.
# Except as permitted by contract or express written permission of Hewlett
# Packard Enterprise Development LP., no part of this work or its content may be
# used, reproduced or disclosed in any form.
#

top_level=$PWD

source ./external/cdst_build_library/build_lib

setup_modules

module load cray-cdst-support
module load cray-cti-devel
#module load cray-mrnet
check_exit_status


# Copy scripts to test dir for execution
cp $top_level/build.sh $top_level/tests/
cp $top_level/run_tests.sh $top_level/tests/
cp $top_level/configure $top_level/tests/

echo "############################################"
echo "#            Running Unit Tests            #"
echo "############################################"

cp $top_level/build.sh $top_level/tests/
check_exit_status

cp $top_level/run_tests.sh $top_level/tests/
check_exit_status

cd $top_level/tests
check_exit_status

./build.sh
check_exit_status

./run_tests.sh -l
check_exit_status

echo "############################################"
echo "#              Done with Tests             #"
echo "############################################"

exit_with_status
