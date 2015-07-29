/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifdef os_linux

#include <sys/syscall.h>

#include "PerfDataEvent.h"
#include "PerfDataSysEvent.h"
#include "xplat_lightweight/Process.h"

#define SEC_PER_JIFFIES (.01)
#define MSEC_PER_JIFFIES (10)

#ifndef SYS_gettid
#define SYS_gettid 224
#endif

int PerfDataSysMgr_get_ThreadTime(long* user, long* sys)
{
  char procFilename[256];
  char buffer[1024];

  pid_t pid = Process_GetProcessId(); 

  static int gettid_not_valid = 0;
  int tid;

  FILE *fd;
  int num_read;
  
  char* ptrUsr = strrchr(buffer, ')') + 1;
  int i;

  if (gettid_not_valid)
    return -1;

  tid = syscall((long int) SYS_gettid);
  if (tid == -1 && errno == ENOSYS) {
    gettid_not_valid = 1;
    return -1;
  }

  sprintf(procFilename, "/proc/%d/task/%d/stat",pid,tid);

  mrn_dbg(5, mrn_printf(FLF,stderr, "Thread ID %d proc filename n%s\n", tid, procFilename));

  fd = fopen(procFilename, "r");
  if (fd == NULL) {
    perror ("fopen()");
    return -1;
  }

  num_read = fread(buffer, (size_t)1, (size_t)1024, fd);
  fclose(fd);
  buffer[num_read]='\0';

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
    static long syspage = 0;
    static double pageKB = 0;

    FILE *fd;
    long vpages = 0;
    long ppages = 0;

    char procFilename[256];

    pid_t pid = Process_GetProcessId(); 

    sprintf(procFilename, "/proc/%d/statm",pid);

    if (!syspage) {
        syspage = sysconf(_SC_PAGESIZE);
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

#endif /* os_linux */
