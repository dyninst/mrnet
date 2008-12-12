
 /***************************************************************************
 *  Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "mrnet/MRNet.h"
#include "Message.h"
#include "PerfDataSysEvent.h"
#include "PerfDataEvent.h"
#include "Protocol.h"
#include "utils.h"

#include <iostream>

#include "xplat/Process.h"
#include "xplat/Thread.h"

#define SEC_PER_JIFFIES (.01)
#define MSEC_PER_JIFFIES (10)
#define SYS_gettid 224

using namespace XPlat;

using namespace std;

namespace MRN {

bool send_PacketToGUI( PacketPtr& connect_pkt, PacketPtr& data_pkt )
{
#if !defined(os_windows)
    char * GUI_hostname_ptr;
    Port iGUIPort;
    connect_pkt->unpack( "%s %uhd", &GUI_hostname_ptr, &iGUIPort ); 
    string strGUIHostName( GUI_hostname_ptr );

    int sock_fd_To_GUI=0;
    if(connectHost(&sock_fd_To_GUI, strGUIHostName, iGUIPort ) == -1){
        cout << "connect_to_GUIhost() failed\n";               
        close( sock_fd_To_GUI );
        return false;
    }

    Message msgToGUI;
    msgToGUI.add_Packet( data_pkt );
    if( msgToGUI.send( sock_fd_To_GUI ) == -1 ) {
        cout << "Message.send failed\n";
        close( sock_fd_To_GUI );
        return false;
    }
    close( sock_fd_To_GUI );
#endif
    return true;
}


bool handle_PerfGuiInit( PacketPtr& connect_pkt )
{
    char * topo_ptr = network->get_NetworkTopology()->get_TopologyStringPtr();
    mrn_dbg( 5, mrn_printf(FLF, stderr, "topology: (%p), \"%s\"\n", topo_ptr, topo_ptr ));

    PacketPtr packetToGUI( new Packet( 0, PROT_GUI_INIT, "%s", topo_ptr ) );
    mrn_dbg( 5, mrn_printf(FLF, stderr, "topology: (%p), \"%s\"\n", topo_ptr, topo_ptr ));
    free( topo_ptr );

    return send_PacketToGUI( connect_pkt, packetToGUI );
}


bool handle_PerfDataCPU( PacketPtr& connect_pkt, Rank my_rank )
{
#if defined(os_linux)
    static bool first_time = true;

    static long long llUser = 0;
    static long long llNice = 0;
    static long long llKernel = 0;
    static long long llIdle = 0;
    static long long llTotal = 0;

    FILE *f;
    if( (f = fopen("/proc/stat", "r")) == NULL ) {
        cout << "open /proc/stat failed\n";
        fflush( stdout );
        return false;
    }
    else
    {
        long long currUser = 0;
        long long currNice = 0;
        long long currKernel = 0;
        long long currIdle = 0;
        long long currTotal = 0;
        /* The very first line should be cpu */
        if((fscanf(f, "cpu %lld %lld %lld %lld",
                   &currUser, &currNice, &currKernel, &currIdle)) != 4) {
            cout << "parse /proc/stat failed\n";
            fflush( stdout );      
            fclose(f);
            return false;
        }
        else {
            fclose(f);
            currTotal = currUser + currNice + currKernel + currIdle;
                
            if( first_time ) {
                llUser = currUser;
                llNice = currNice;
                llKernel = currKernel;
                llIdle = currIdle;
                llTotal = currTotal;
                first_time = false;
                return true;
            }
            else {

                float fUserCPUPercent = 100.0;
                float fNiceCPUPercent = 100.0;
                float fKernelCPUPercent = 100.0;
                float fIdleCPUPercent = 100.0;
                float denom = ((float)currTotal - (float)llTotal);

                fUserCPUPercent *= ((float)currUser - (float)llUser) / denom; 
                fNiceCPUPercent *= ((float)currNice - (float)llNice) / denom; 
                fKernelCPUPercent *= ((float)currKernel - (float)llKernel) / denom; 
                fIdleCPUPercent *= ((float)currIdle - (float)llIdle) / denom;

                llUser = currUser;
                llNice = currNice;
                llKernel = currKernel;
                llIdle = currIdle;
                llTotal = currTotal;

                PacketPtr packetToGUI( new Packet( 0, PROT_GUI_CPUPERCENT, "%f %f %f %f %ud",
                                                   fUserCPUPercent, fNiceCPUPercent, fKernelCPUPercent,
                                                   fIdleCPUPercent, my_rank ) );
                
                return send_PacketToGUI( connect_pkt, packetToGUI );
            }
        }
    }
#endif // defined(os_linux)

    return true;
}

int PerfDataSysMgr::get_ThreadTime(long &user, long &sys)
{
	char procFilename[256] ;
	char buffer[1024] ;

	pid_t pid = :: XPlat::Process::GetProcessId();


   	static int gettid_not_valid = 0;
   	int tid;

   	if (gettid_not_valid)
      		return getpid();

#if defined(os_linux)	

   	tid = syscall((long int) SYS_gettid);
   	if (tid == -1 && errno == ENOSYS)
   	{
      		gettid_not_valid = 1;
      		return getpid();
   	}

	sprintf(procFilename, "/proc/%d/task/%d/stat",pid,tid) ;
	
        mrn_dbg( 5, mrn_printf(FLF, stderr, " ThreadID  %d proc  filename n%s ",  tid, procFilename ));

	FILE *fd;
	int num_read ;
	fd = fopen(procFilename, "r");
	if ( fd == NULL) {
		:: perror ("fopen()");
		return -1;
		}
	num_read = fread(buffer,1, 1023, fd);
	fclose(fd);
	buffer[num_read] = '\0';


	char* ptrUsr = strrchr(buffer, ')') + 1 ;
	for(int i = 3 ; i != 14 ; ++i) ptrUsr = strchr(ptrUsr+1, ' ') ;

	ptrUsr++;
	user = (atol(ptrUsr)) ;
	user = user * MSEC_PER_JIFFIES ;
	sys = atol(strchr(ptrUsr,' ') + 1) ;
	sys = sys * MSEC_PER_JIFFIES;

#endif // define (os_linux)

	return 1;
}

int  PerfDataSysMgr::get_MemUsage(double &vsize, double &psize)
{
	char procFilename[256] ;
	char buffer[1024] ;
	pid_t pid = :: XPlat::Process::GetProcessId();

#if defined(os_linux)	

	sprintf(procFilename, "/proc/%d/statm",pid) ;
	
	FILE *fd;
	long vpages =0;
	long ppages =0;
	static double pagesize = 0;
	if (pagesize == 0) {
		pagesize = sysconf(_SC_PAGESIZE);
		pagesize = pagesize/1024;
	}

	fd = fopen(procFilename, "r");
	if ( fd == NULL) {
		:: perror ("fopen()");
		return -1;
	}


 	/* The very first line should be memory usage */
        if((fscanf(fd, "%ld %ld",
                   &vpages, &ppages)) != 2) {
            mrn_dbg  (1,  mrn_printf (FLF, stderr, "parse /proc/stat failed\n"));
            fflush( stdout );
            fclose(fd);
            return -1;
        } else {
		fclose(fd);
		vsize = vpages * pagesize;
		psize = ppages * pagesize;
        	mrn_dbg( 5, mrn_printf(FLF, stderr, " proc file name %s \n vpages %ld  ppages  %ld pagesize  %f \n vsize %f psize %f \n", procFilename, vpages, ppages, pagesize, vsize, psize));
	}

#endif // define (os_linux)

	return 1;
}


}
