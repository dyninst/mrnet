#if !defined __error_h
#define __error_h 1

char* Error_GetErrorString(int code);

int Error_ETimedOut(int code);

int Error_EAddrInUse(int code);

int Error_EConnRefused(int code);

#endif /* __error_h */
