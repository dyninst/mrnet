#!/bin/ksh
#PBS -N launch_cray
#PBS -j oe
#PBS -l nodes=10
#PBS -l walltime=0:10:00

#echo -------------------- Environment ----------------------
#env
#echo -------------------------------------------------------

echo ---------------------- Job Info -----------------------
qstat -f $PBS_JOBID
echo -------------------------------------------------------
echo $MRNET_SCRATCH_DIR

source /opt/modules/default/init/ksh
cd $MRNET_SCRATCH_DIR
export PATH=$MRNET_INSTALL_DIR/bin:$PATH
export LD_LIBRARY_PATH=$MRNET_INSTALL_DIR/lib:$LD_LIBRARY_PATH

topodef=10_node_topo.def

## Build Topology
hostfile=hosts.txt
fehost=$(cat /proc/cray_xt/nid | awk '{printf("nid%05u\n", $1); }')
aprun -n 10 -N 1 /bin/hostname | sort | tail -n +2 | awk -v ncpus=16 '{printf("%s:%d\n", $1, ncpus)}' > $hostfile

python build_topo.py $hostfile $topodef $fehost
echo $MRNET_INSTALL_DIR/x86_64-cray-linux-gnu/bin/IntegerAddition_FE $MRNET_SCRATCH_DIR/$topodef.top $MRNET_INSTALL_DIR/x86_64-cray-linux-gnu/bin/IntegerAddition_BE $MRNET_INSTALL_DIR/x86_64-cray-linux-gnu/lib/IntegerAdditionFilter.so
$MRNET_INSTALL_DIR/x86_64-cray-linux-gnu/bin/IntegerAddition_FE $MRNET_SCRATCH_DIR/$topodef.top $MRNET_INSTALL_DIR/x86_64-cray-linux-gnu/bin/IntegerAddition_BE $MRNET_INSTALL_DIR/x86_64-cray-linux-gnu/lib/IntegerAdditionFilter.so