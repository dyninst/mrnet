/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifdef os_linux

#include <cerrno>
#include <sys/syscall.h>

#include "PerfDataSysEvent.h"
#include "PerfDataEvent.h"
#include "xplat/Process.h"

#define SEC_PER_JIFFIES (.01)
#define MSEC_PER_JIFFIES (10)

#ifndef SYS_gettid
#define SYS_gettid 224
#endif

using namespace std;

namespace MRN {

int PerfDataSysMgr::get_ThreadTime(long &user, long &sys)
{
    char procFilename[256] ;
    char buffer[1024] ;

    pid_t pid = XPlat::Process::GetProcessId();


    static int gettid_not_valid = 0;
    long int tid;

    if (gettid_not_valid)
        return -1;

    tid = syscall(SYS_gettid);
    if (tid == -1 && errno == ENOSYS) {
        gettid_not_valid = 1;
        return -1;
    }

    sprintf( procFilename, "/proc/%d/task/%ld/stat", pid, tid );
	
    FILE *fd;
    size_t num_read ;
    fd = fopen(procFilename, "r");
    if ( fd == NULL) {
        ::perror("fopen()");
        mrn_dbg( 5, mrn_printf(FLF, stderr, "failed to open %s\n", 
                               procFilename) );
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

    return 0;
}

int PerfDataSysMgr::get_MemUsage(double &vsize, double &psize)
{
    static long syspage = 0;
    static double pageKB = 0;

    FILE *fd;
    long vpages =0;
    long ppages =0;

    char procFilename[256] ;
    pid_t pid = XPlat::Process::GetProcessId();

    sprintf( procFilename, "/proc/%d/statm", pid );
	
    if( ! syspage ) {
        syspage = sysconf(_SC_PAGESIZE);
        pageKB = (double)syspage/1024;
    }

    fd = fopen(procFilename, "r");
    if( fd == NULL) {
        ::perror("fopen()");
        mrn_dbg( 5, mrn_printf(FLF, stderr, "failed to open %s\n", 
                               procFilename) );
        return -1;
    }

    /* The very first line should be memory usage */
    if( fscanf(fd, "%ld %ld", &vpages, &ppages) != 2 ) {
        mrn_dbg  (1,  mrn_printf (FLF, stderr, "parse /proc/stat failed\n"));
        fflush( stdout );
        fclose(fd);
        return -1;
    } else {
        fclose(fd);
        vsize = (double)vpages * pageKB;
        psize = (double)ppages * pageKB;
        mrn_dbg( 5, mrn_printf(FLF, stderr, 
                               "file=%s : vpages=%ld ppages=%ld pagesize=%lf vsize=%lf psize=%lf\n", 
                               procFilename, vpages, ppages, pageKB, vsize, psize));
    }

    return 0;
}


} /* namespace MRN */

#endif /* os_linux */
