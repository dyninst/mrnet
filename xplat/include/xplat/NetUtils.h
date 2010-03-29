/****************************************************************************
 * Copyright © 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

// $Id: NetUtils.h,v 1.7 2008/10/09 19:53:51 mjbrim Exp $
#ifndef XPLAT_NETUTILS_H
#define XPLAT_NETUTILS_H

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if !defined(os_windows)
# include <netinet/in.h>
#else
# include <winsock2.h>
#endif

#include "xplat/Types.h"

namespace XPlat
{

class NetUtils
{
public:
    class NetworkAddress
    {
    private:
        bool isv6;               // true if IPv6
        std::string str;         // IP address as string
        in_addr_t ip4addr;       // IPv4 address in host byte order
#ifndef os_windows
        struct in6_addr ip6addr; // IPv6 address in network byte order
#endif

    public:
        NetworkAddress( ): str("")
        {
            isv6 = false;
            ip4addr = ntohl(INADDR_ANY);
#ifndef os_windows
            ip6addr = in6addr_any;
#endif
        }
        NetworkAddress( in_addr_t inaddr );
        NetworkAddress( struct sockaddr_in6* inaddr );
        NetworkAddress( const NetworkAddress& obj )
          : isv6( obj.isv6 ), str( obj.str )
        { 
            if( ! isv6 )
                ip4addr = obj.ip4addr;
#ifndef os_windows
            else
                memcpy( (void*)&ip6addr,
                        (const void*)&obj.ip6addr,
                        sizeof(struct in6_addr) );
#endif
        }

        NetworkAddress&
        operator=( const NetworkAddress& obj )
        {
            if( &obj != this )
            {
                isv6 = obj.isv6;
                str = obj.str;
                if( ! isv6 )
                    ip4addr = obj.ip4addr;
#ifndef os_windows
                else
                    memcpy( (void*)&ip6addr,
                            (const void*)&obj.ip6addr,
                            sizeof(struct in6_addr) );
#endif
            }
            return *this;
        }

        bool operator==( const NetworkAddress & in )
        { 
            if( ! isv6 )
                return ip4addr == in.ip4addr;
#ifndef os_windows
            else
                return 0 == memcmp( (const void*)&ip6addr,
                                    (const void*)&in.ip6addr,
                                    sizeof(struct in6_addr) );
#endif
        }

	bool IsV4( void ) const { return !isv6; } 
	bool IsV6( void ) const { return isv6; } 
        std::string GetString( void ) const { return str; }
        in_addr_t GetInAddr( void ) const { return ip4addr; }
        void GetIn6Addr( struct in6_addr* inaddr ) const 
        {
#ifndef os_windows 
            if( inaddr != NULL )
                memcpy( (void*)inaddr,
                        (const void*)&ip6addr,
                        sizeof(struct in6_addr) );
#endif
        }
    };
private:
    static int FindNetworkName( std::string ihostname, std::string& );
    static int FindNetworkAddress( std::string ihostname, NetworkAddress& );
    static int FindNumberOfLocalNetworkInterfaces( void ); 
    static int FindLocalNetworkInterfaces( std::vector< NetworkAddress >& );

public:
    static int GetHostName( std::string ihostname, std::string& );
    static int GetNetworkName( std::string ihostname, std::string& );
    static int GetNetworkAddress( std::string ihostname, NetworkAddress& );
    static int GetNumberOfNetworkInterfaces( void );
    static int GetLocalNetworkInterfaces( std::vector< NetworkAddress >& );
    static int GetLocalHostName( std::string& this_host );

    // check whether given host is local
    static bool IsLocalHost( const std::string& host );

    static int GetLastError( void );
};

} // namespace XPlat

#endif // XPLAT_NETUTILS_H
