/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(os_windows)
#include <sys/syscall.h>
#include <unistd.h>
#else
#include <winsock2.h>
#endif //!defined(os_windows)
#include <errno.h>

#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"
#include "mrnet_lightweight/Types.h"
#include "utils_lightweight.h"

#include "xplat_lightweight/Process.h"

#define SYS_gettid 224
#define SEC_PER_JIFFIES (.01)
#define MSEC_PER_JIFFIES (10)

int PerfDataSysMgr_get_ThreadTime(long* user, long* sys)
{
  char procFilename[256];
  char buffer[1024];

  int tid;
  FILE *fd;
  int num_read;

  char* ptrUsr;
  int i;

  int pid;
  static int gettid_not_valid;
  pid = Process_GetProcessId();
  gettid_not_valid = 0;

  if (gettid_not_valid)
    return -1;

  //tid = syscall((long int) SYS_gettid);
  //if (tid == -1 && errno == ENOSYS) {
  //  gettid_not_valid = 1;
  //  return -1;
  //}

  tid = (int)GetCurrentThreadId();
  // TODO: figure out how to error check tid

  sprintf(procFilename, "/proc/%d/task/%d/stat",pid,tid);

  mrn_dbg(5, mrn_printf(FLF,stderr, "Thread ID %d proc filename n%s\n", tid, procFilename));

  fd = fopen(procFilename, "r");
  if (fd == NULL) {
    perror ("fopen()");
    return -1;
  }

  num_read = fread(buffer,1,1024, fd);
  fclose(fd);
  buffer[num_read]='\0';

  ptrUsr = strrchr(buffer, ')') + 1;
  for (i = 3; i != 14; ++i) ptrUsr = strchr(ptrUsr+1, ' ');

  ptrUsr++;
  *user = (atol(ptrUsr));
  *user = (*user) * MSEC_PER_JIFFIES;
  *sys = atol(strchr(ptrUsr,' ') + 1);
  *sys = (*sys)*MSEC_PER_JIFFIES;

  return 0;

}

int PerfDataSysMgr_get_MemUsage(double* vsize, double* psize) 
{
    static unsigned int syspage = 0;
    static double pageKB = 0;

    FILE *fd;
    long vpages = 0;
    long ppages = 0;

    char procFilename[256];

    int pid;
	pid = Process_GetProcessId(); 

    sprintf(procFilename, "/proc/%d/statm",pid);

    if (!syspage) {
        //syspage = sysconf(_SC_PAGESIZE);
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		syspage = si.dwPageSize;
        pageKB = (double)syspage/1024;
    }

    fd = fopen(procFilename, "r");
    if (fd == NULL) {
        perror("fopen()");
        return -1;
    }

    /* This very first line should be memory usage */
    if ((fscanf(fd, "%ld %ld",
                    &vpages, &ppages)) != 2) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "parse /proc/stat failed\n"));

        fflush(stdout);
        fclose(fd);
        return -1;
    } else {
        fclose(fd);
        *vsize = vpages * pageKB;
        *psize = ppages * pageKB;
        mrn_dbg(5, mrn_printf(FLF, stderr, "file=%s \n vpages=%ld ppages=%ld pagesize=%ld \n vsize=%lf psize=%lf\n", procFilename, vpages, ppages, pageKB, *vsize, *psize));  
    }

    return 0;
    
}
