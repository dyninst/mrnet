/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: NetUtils.C,v 1.11 2008/10/09 19:54:04 mjbrim Exp $

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include "xplat/Types.h"
#include "xplat/NetUtils.h"
#include "xplat/PathUtils.h"

#if defined(os_windows)
#include <ws2tcpip.h>
#endif /* os_windows */

namespace XPlat
{

static bool checked_resolve_env = false;
static bool use_resolve = true;
static bool use_canonical = false;

void get_resolve_env(void)
{
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
}

#define XPLAT_MAX_HOSTNAME_LEN 256

int NetUtils::FindNetworkName( std::string ihostname, std::string & ohostname )
{
    struct addrinfo *addrs, hints;
    int error;

    if( ihostname == "" )
        return -1;

    get_resolve_env();

    if( use_resolve ) {
        // do the lookup
        memset(&hints, 0, sizeof(hints));
        if( use_canonical )
            hints.ai_flags = AI_CANONNAME;
        hints.ai_socktype = SOCK_STREAM;
        if( error = getaddrinfo(ihostname.c_str(), NULL, &hints, &addrs) ) {
            fprintf(stderr, "%s[%d]: getaddrinfo(%s): %s\n", 
                    __FILE__, __LINE__,
                    ihostname.c_str(), gai_strerror(error));
            return -1;
        }

        char hostname[XPLAT_MAX_HOSTNAME_LEN];
        if( use_canonical && (addrs->ai_canonname != NULL) ) {
            strncpy( hostname, addrs->ai_canonname, sizeof(hostname) );
            hostname[XPLAT_MAX_HOSTNAME_LEN-1] = '\0';
        }
        else {
            if( addrs->ai_family == AF_INET6 ) {
#ifndef os_windows
                if( error = getnameinfo(addrs->ai_addr, sizeof(struct sockaddr_in6), 
                                        hostname, sizeof(hostname), NULL, 0, 0) ) {
                    fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                    return -1;
                }
#endif
            }
            else if( addrs->ai_family == AF_INET ) {
                if( error = getnameinfo(addrs->ai_addr, sizeof(struct sockaddr_in), 
                                        hostname, sizeof(hostname), NULL, 0, 0) ) {
                    fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
                    return -1;
                }
            }
            else
                strncpy( hostname, ihostname.c_str(), sizeof(hostname) );
        }
        ohostname = hostname;

        freeaddrinfo(addrs);
    }
    else
        ohostname = ihostname;

    return 0;
}

bool NetUtils::IsLocalHost( const std::string& ihostname )
{
    std::vector< NetworkAddress > local_addresses;
    GetLocalNetworkInterfaces( local_addresses );

    NetworkAddress iaddress;
    if( GetNetworkAddress( ihostname, iaddress ) == -1 ){
        return false;
    }

    for( unsigned int i=0; i<local_addresses.size(); i++ ) {
        if( local_addresses[i] == iaddress )
            return true;
    }

    return false;
}


int NetUtils::GetHostName( std::string ihostname, std::string &ohostname )
{

    std::string fqdn;
    if( FindNetworkName( ihostname, fqdn ) == -1 ){
        return -1;
    }

    // extract host name from the fully-qualified domain name
    std::string::size_type firstDotPos = fqdn.find_first_of( '.' );
    if( firstDotPos != std::string::npos )
        ohostname = fqdn.substr( 0, firstDotPos );
    else 
        ohostname = fqdn;

    return 0;
}


int NetUtils::GetNetworkName( std::string ihostname, std::string & ohostname )
{
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
    // find the dotted decimal form of the address

    // get address in network byte order
    in_addr_t nboaddr = htonl( ip4addr );

    // access the address as an array of bytes
    const unsigned char* cp = (const unsigned char*)&nboaddr;

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
    : isv6(true), ip4addr(ntohl(INADDR_NONE))
{
#ifndef os_windows
    if( NULL != inaddr ) {
        memcpy( (void*)&ip6addr,
                (void*)&inaddr->sin6_addr,
                sizeof(struct in6_addr) );
    }

    // find the hex form of the address
    char hostname[64];
    int error;
    if( error = getnameinfo((struct sockaddr*)inaddr, sizeof(struct sockaddr_in6), 
			    hostname, sizeof(hostname), NULL, 0, NI_NUMERICHOST) ) {
        fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(error));
    }
    hostname[63] = '\0';
    str = hostname;
#endif
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

int NetUtils::FindNetworkAddress( std::string ihostname, NetUtils::NetworkAddress &oaddr )
{
    struct addrinfo *addrs, *addrs_iter, hints;
    int error;

    if( ihostname == "" )
        return -1;

    get_resolve_env();

    // do the lookup
    memset( &hints, 0, sizeof(hints) );
    if( use_canonical )
        hints.ai_flags = AI_CANONNAME;
    hints.ai_socktype = SOCK_STREAM;
    if( error = getaddrinfo(ihostname.c_str(), NULL, &hints, &addrs) ) {
        fprintf(stderr, "%s[%d]: getaddrinfo(%s): %s\n", 
                __FILE__, __LINE__,
                ihostname.c_str(), gai_strerror(error));
        return -1;
    }

    addrs_iter = addrs;
    while( addrs_iter != NULL ) {
        if( addrs_iter->ai_family == AF_INET ) {
            struct in_addr in;
	    struct sockaddr_in *sinptr = ( struct sockaddr_in * )(addrs_iter->ai_addr);
	    memcpy( &in.s_addr, ( void * )&( sinptr->sin_addr ), sizeof( in.s_addr ) );
	    oaddr = NetworkAddress( ntohl(in.s_addr) );
	    break;
	}
	addrs_iter = addrs_iter->ai_next;
    }

    freeaddrinfo(addrs);
    return 0;
}

} // namespace XPlat
