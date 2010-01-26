#if !defined __netutils_h
#define __netutils_h 1


#if !defined(os_windows)
#include <netinet/in.h>
#else
#include <winsock2.h>
#endif



void get_resolve_env();

int NetUtils_FindNetworkName(char* ihostname, char* ohostname);

int NetUtils_GetHostName(char* ihostname, char* ohostname);

// has -unix and -win versions
int NetUtils_GetLastError();
int NetUtils_GetLocalHostName(char* this_host);
#endif /* __netutils_h */
