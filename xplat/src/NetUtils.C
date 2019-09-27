/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: NetUtils.C,v 1.11 2008/10/09 19:54:04 mjbrim Exp $

#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "xplat/Types.h"
#include "xplat/Mutex.h"
#include "xplat/NetUtils.h"
#include "xplat/PathUtils.h"

#if defined(os_windows)
#include <ws2tcpip.h>
#endif /* os_windows */

#if defined(os_solaris)
#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif
#endif

namespace XPlat
{

static bool checked_resolve_env = false;
static bool use_resolve = true;
static bool use_canonical = false;

void get_resolve_env(void)
{
#ifndef arch_crayxt
    if( ! checked_resolve_env ) {
        const char* varval = getenv( "XPLAT_RESOLVE_HOSTS" );
        if( varval != NULL ) {
            if( strcmp("0", varval) )
                use_resolve = true;
            else
                use_resolve = false;
        }

        if( use_resolve ) {
            varval = getenv( "XPLAT_RESOLVE_CANONICAL" );
            if( varval != NULL ) {               
                if( strcmp("0", varval) )
                    use_canonical = true;
                else
                    use_canonical = false;
            }
        }
        checked_resolve_env = true;
    }
#else
    use_resolve = false;
#endif
}

static Mutex gha_lock;
struct addrinfo* get_host_addrs( const std::string& ihost )
{
    const char* ihostname = ihost.c_str();
    struct addrinfo hints;
    struct addrinfo *addrs = NULL;
    int error;

    // do the lookup
    gha_lock.Lock();
    memset(&hints, 0, sizeof(hints));
    if( use_canonical )
        hints.ai_flags = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    if( (error = getaddrinfo(ihostname, NULL, &hints, &addrs)) ) {
        gha_lock.Unlock();
        fprintf(stderr, "%s[%d]: getaddrinfo(%s): %s\n", 
                __FILE__, __LINE__,
                ihostname, gai_strerror(error));
        return NULL;
    }
    gha_lock.Unlock();

    return addrs;
}

bool NetUtils::IsIPAddressStr( std::string& iaddr )
{
    const char* addrstr = iaddr.c_str();
    size_t len = iaddr.length();

    char* str = const_cast<char*>( addrstr );
    char* sep;
    char* beg = str;
    if( (sep = strchr(beg, (int)'.')) != NULL ) {
        // IPv4 -  examine four segments
        unsigned nseg = 1;
        do {
            // check segment is at most 3 digits
            size_t run = sep - beg;
            if( run > 3 ) {
                return false;
            }

            // each segment must be a val in [0, 255]
            if( isdigit(*beg) ) {
                char* endptr = NULL;
                long int val = strtol(beg, &endptr, 10);
                if( endptr != sep ) {
                    return false;
                }
                if( (val < 0) || (val > 255) ) {
                    return false;
                }
            }
            else {
                return false;
            }

            // get next segment
            beg = sep + 1;
            if( (size_t)(beg - str) > len )
                break;
            nseg++;
            sep = strchr(beg, (int)'.');
            if( sep == NULL )
                sep = str + len;
        } while( nseg <= 4 );

        // check that we saw exactly 4 segments
        if( nseg != 4 ) {
            return false;
        }

        return true;
    }
    else if( (sep = strchr(beg, (int)':')) != NULL ) {
        // IPv6 - there can be up to eight segments
        long int max = 0x0FFFF;
        unsigned nseg = 1;
        do {
            // check segment is at most 4 digits
            size_t run = sep - beg;
            if( run > 4 ) {
                return false;
            }

            // each segment must be a val in [0, 0xFFFF]
            if( run > 0 ) {
                if( isxdigit(*beg) ) {
                    char* endptr = NULL;
                    long int val = strtol(beg, &endptr, 16);
                    if( endptr != sep ) {
                        return false;
                    }
                    if( (val < 0) || (val > max) ) {
                        return false;
                    }
                }
                else return false;
            }

            // get next segment
            beg = sep + 1;
            if( (size_t)(beg - str) > len )
                break;
            if( *beg == ':' )
                beg++;
            nseg++;
            sep = strchr(beg, (int)':');
            if( sep == NULL )
                sep = str + len;
        } while( nseg <= 8 );

        // check that we didn't see too many segments
        if( nseg > 8 ) {
            return false;
        }
    }
    return false;
}

int NetUtils::FindNetworkName( const std::string& ihostname, std::string& ohostname )
{
    if( ihostname == "" )
        return -1;

    get_resolve_env();
    if( use_resolve ) {
        struct addrinfo *addrs = NULL;
        int error;
        
        addrs = get_host_addrs( ihostname );
        if( addrs == NULL ) {
            return -1;
        }

        char* hostname = (char*) calloc( XPLAT_MAX_HOSTNAME_LEN, sizeof(char) );
        if( use_canonical && (addrs->ai_canonname != NULL) ) {
            strncpy( hostname, addrs->ai_canonname, XPLAT_MAX_HOSTNAME_LEN );
            hostname[XPLAT_MAX_HOSTNAME_LEN-1] = '\0';
        }
        else {
            if( addrs->ai_family == AF_INET6 ) {
#ifndef os_windows
                if( (error = getnameinfo(addrs->ai_addr, sizeof(struct sockaddr_in6),
                                        hostname, XPLAT_MAX_HOSTNAME_LEN, NULL, 0, 0)) ) {
                    fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                    return -1;
                }
#endif
            }
            else if( addrs->ai_family == AF_INET ) {
                if( (error = getnameinfo(addrs->ai_addr, sizeof(struct sockaddr_in),
                                        hostname, XPLAT_MAX_HOSTNAME_LEN, NULL, 0, 0)) ) {
                    fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                    return -1;
                }
            }
        }
        freeaddrinfo(addrs);

        if( strlen(hostname) ) {
            ohostname = hostname;
            free( hostname );
            return 0;
        }
        free( hostname );
    }

    ohostname = ihostname;
    return 0;
}

bool NetUtils::IsLocalHost( const std::string& ihostname )
{

#ifndef arch_crayxt

    std::vector< NetworkAddress > local_addresses;
    GetLocalNetworkInterfaces( local_addresses );
    
    if( local_addresses.size() == 0 ) {
        fprintf(stderr, "%s[%d]: GetLocalNetworkInterfaces() returned no addresses\n",
                __FILE__, __LINE__);
        return false;
    }

    NetworkAddress iaddress;
    if( GetNetworkAddress( ihostname, iaddress ) == -1 ){
        return false;
    }

    if (iaddress.IsLocal() == true)
        return true;

    for( unsigned int i=0; i<local_addresses.size(); i++ ) {
        if( local_addresses[i] == iaddress )
            return true;
    }

#else

    std::string localhost;
    NetUtils::GetLocalHostName( localhost );
    if( (ihostname == localhost) || 
	(ihostname == "localhost") )
        return true;

#endif

    return false;
}


int NetUtils::GetHostName( std::string ihostname, std::string &ohostname )
{
    if( IsIPAddressStr(ihostname) ) {
        ohostname = ihostname;
        return 0;
    }

    // get fully-qualified domain name
    std::string fqdn;
    if( -1 == FindNetworkName(ihostname, fqdn) )
        return -1;

    // extract host name from FQDN
    std::string::size_type firstDotPos = fqdn.find_first_of( '.' );
    if( firstDotPos != std::string::npos )
        ohostname = fqdn.substr( 0, firstDotPos );
    else 
        ohostname = fqdn;

    return 0;
}


int NetUtils::GetNetworkName( std::string ihostname, std::string & ohostname )
{
    if( IsIPAddressStr(ihostname) ) {
        ohostname = ihostname;
        return 0;
    }

    return FindNetworkName( ihostname, ohostname );
}

int NetUtils::GetNetworkAddress( std::string ihostname, NetworkAddress & oaddr )
{
    return FindNetworkAddress( ihostname, oaddr  );
}

// Note: does not use inet_ntoa or similar functions because they are not
// necessarily thread safe, or not available on all platforms of interest.
NetUtils::NetworkAddress::NetworkAddress( in_addr_t inaddr )
    : isv6(false), ip4addr( inaddr )
{
    // find the dotted decimal form of the address by
    // accessing the address as an array of bytes
    const unsigned char* cp = (const unsigned char*)&ip4addr;

    std::ostringstream astr;
    astr << (unsigned int)cp[0] 
        << '.' << (unsigned int)cp[1] 
        << '.' << (unsigned int)cp[2] 
        << '.' << (unsigned int)cp[3];

    str = astr.str();
}

// Note: does not use inet_ntoa or similar functions because they are not
// necessarily thread safe, or not available on all platforms of interest.
NetUtils::NetworkAddress::NetworkAddress( struct sockaddr_in6* inaddr )
    : isv6(true), ip4addr(INADDR_NONE)
{
    if( NULL != inaddr ) {
        memcpy( (void*)&ip6addr,
                (void*)&inaddr->sin6_addr,
                sizeof(struct in6_addr) );
    }

    // find the hex form of the address
    char hostname[64];
    int error;
    if( (error = getnameinfo((struct sockaddr*)inaddr, sizeof(struct sockaddr_in6),
			    hostname, sizeof(hostname), NULL, 0, NI_NUMERICHOST)) ) {
        fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
    }
    hostname[63] = '\0';
    str = hostname;
}

int NetUtils::GetNumberOfNetworkInterfaces( void )
{
    static int cachedNumberOfInterfaces=0;

    if( cachedNumberOfInterfaces == 0 ) {
        cachedNumberOfInterfaces = FindNumberOfLocalNetworkInterfaces( );
    }

    return cachedNumberOfInterfaces;
}

int NetUtils::GetLocalNetworkInterfaces( std::vector<NetUtils::NetworkAddress> & iaddresses )
{
    static std::vector<NetUtils::NetworkAddress> cachedLocalAddresses;

    if( cachedLocalAddresses.size() == 0 ){
        int ret = FindLocalNetworkInterfaces( cachedLocalAddresses );
        if( ret == -1 )
            return -1;
    } 

    iaddresses = cachedLocalAddresses;
    return 0;
}

int NetUtils::FindNetworkAddress( const std::string& ihostname, NetUtils::NetworkAddress& oaddr )
{
    struct addrinfo *addrs, *addrs_iter;

    if( ihostname == "" )
        return -1;

    get_resolve_env();

    addrs = get_host_addrs( ihostname );
    if( addrs != NULL ) {
        addrs_iter = addrs;
        while( addrs_iter != NULL ) {
            if( addrs_iter->ai_family == AF_INET ) {
                struct in_addr in;
		struct sockaddr_in *sinptr = ( struct sockaddr_in * )(addrs_iter->ai_addr);
		memcpy( &in.s_addr, ( void * )&( sinptr->sin_addr ), sizeof( in.s_addr ) );
		oaddr = NetworkAddress( in.s_addr );
		break;
	    }
	    addrs_iter = addrs_iter->ai_next;
	}
	freeaddrinfo(addrs);
    }

    return 0;
}

} // namespace XPlat
