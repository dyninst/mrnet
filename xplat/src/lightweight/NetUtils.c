/****************************************************************************
 * Copyright © 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xplat_lightweight/NetUtils.h"

#if defined(os_windows)
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif /* os_windows */

static int checked_resolve_env = 0;
static int use_resolve = 1;
static int use_canonical = 0;

#define XPLAT_MAX_HOSTNAME_LEN 256

void get_resolve_env()
{
  const char* varval;

  if ( ! checked_resolve_env ) {
    varval = getenv( "XPLAT_RESOLVE_HOSTS" );
    if (varval != NULL ) {
      if (strcmp("0", varval) )
        use_resolve = 1;
      else
        use_resolve = 0;
    } 
    if ( use_resolve ) {
      varval = getenv( "XPLAT_RESOLVE_CANONICAL" );
      if ( varval != NULL) {
        if (strcmp("0", varval) )
          use_canonical = 1;
        else
          use_canonical = 0;
      }
    }
    checked_resolve_env = 1;
  }
}

int NetUtils_FindNetworkName( char* ihostname, char* ohostname)
{
  struct addrinfo *addrs, hints;
  int error;
  char hostname[XPLAT_MAX_HOSTNAME_LEN];

  if ( ihostname == NULL )
    return -1;

  get_resolve_env();

  if ( use_resolve ) {
    // do the lookup
    memset(&hints, 0, sizeof(hints));
    if ( use_canonical) 
      hints.ai_flags = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(ihostname, NULL, &hints, &addrs);
    if ( error ) {
        fprintf(stderr, "%s[%d]: getaddrinfo(%s): %s\n",
                __FILE__, __LINE__,
                ihostname, gai_strerror(error));
        return -1;
    }
  
    if ( use_canonical && ( addrs->ai_canonname != NULL) ) {
      strncpy( hostname, addrs->ai_canonname, sizeof(hostname));
      hostname[XPLAT_MAX_HOSTNAME_LEN-1] = '\0';
    }
    else {
        if (addrs->ai_family == AF_INET6) {
            error = getnameinfo(addrs->ai_addr, (socklen_t)(sizeof(struct sockaddr_in6)),
                                hostname, XPLAT_MAX_HOSTNAME_LEN, NULL, 0, 0);
            if ( error ) {
	        fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                return -1;
            }
        }
        else if (addrs->ai_family == AF_INET) {
            error = getnameinfo(addrs->ai_addr, (socklen_t)(sizeof(struct sockaddr_in)),
                                hostname, XPLAT_MAX_HOSTNAME_LEN, NULL, 0, 0);
	    if ( error ) {
                fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                return -1;
            }
        }
    }
    strncpy(ohostname, hostname, (size_t)XPLAT_MAX_HOSTNAME_LEN);
  }
  else {
    strncpy(ohostname, ihostname, (size_t)XPLAT_MAX_HOSTNAME_LEN);
  }
  return 0; 
}

int NetUtils_GetHostName( char* ihostname, char* ohostname )
{
  char* tok;
  const char* delim = ".";
  char* fqdn = (char*) malloc((size_t)XPLAT_MAX_HOSTNAME_LEN);
  assert(fqdn);
  if ( NetUtils_FindNetworkName(ihostname, fqdn) == -1 ) {
    free(fqdn);
    return -1;
  }

  // extract host name from the fully-qualified domain name
  // find first occurence of "."
  // if substring does not equal the entire string

  tok = strtok(fqdn, delim);
  if (!strcmp(fqdn, tok))
    strncpy(ohostname, tok, (size_t)XPLAT_MAX_HOSTNAME_LEN);
  else 
    strncpy(ohostname,fqdn, (size_t)XPLAT_MAX_HOSTNAME_LEN);

  return 0;
}
