/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__packet_h)
#define __packet_h 1

#include <cstdarg>
#include <set>
#include <vector>

#include "boost/shared_ptr.hpp"
#include "mrnet/DataElement.h"
#include "mrnet/Error.h"
#include "xplat/Mutex.h"

struct PDR;

namespace MRN
{

class Packet;
typedef boost::shared_ptr< Packet > PacketPtr;

class Packet: public Error {

    friend class ParentNode;
    friend class BackEndNode;
    friend class Stream;
    friend class Message;
    friend class Network;

 public:

    // BEGIN MRNET API

    static PacketPtr NullPacket;

    Packet( unsigned int istream_id, int itag, const char *ifmt, ... );
    Packet( const char *ifmt, va_list idata, unsigned int istream_id, int itag );
    Packet( unsigned int istream_id, int itag, const void **idata, const char *ifmt );

    int unpack( const char *ifmt, ... );
    const DataElement* operator[]( unsigned int i ) const;

    int get_Tag(void) const;
    void set_Tag( int itag ) { tag = itag; }

    unsigned int get_StreamId(void) const;
    const char* get_FormatString(void) const;
    Rank get_InletNodeRank(void) const;
    Rank get_SourceRank(void) const;

    bool set_Destinations( const Rank* bes, unsigned int num_bes );

    bool operator==( const Packet & ) const;
    bool operator!=( const Packet & ) const;

    void set_DestroyData( bool b );

    // END MRNET API

    ~Packet();

 private:

    Packet( Rank isrc, unsigned int istream_id, int itag, 
            const char *ifmt, va_list idata );
    Packet( Rank isrc, unsigned int istream_id, int itag, 
            const void **idata, const char *ifmt );
    Packet( unsigned int ihdr_len, char *ihdr, 
            unsigned int ibuf_len, char *ibuf, 
            Rank iinlet_rank );

    void encode_pdr_header(void);
    void encode_pdr_data(void);
    void decode_pdr_header(void) const;
    void decode_pdr_data(void) const;

    const char *get_Buffer(void) const;
    unsigned int get_BufferLen(void) const;
    const char *get_Header(void) const;
    unsigned int get_HeaderLen(void) const;

    bool get_Destinations( unsigned& num_dest, Rank** dests );

    void set_SourceRank( Rank r ) { src_rank = r; }

    unsigned int get_NumDataElements(void) const;
    const DataElement * get_DataElement( unsigned int i ) const;

    int ExtractVaList( const char *fmt, va_list arg_list ) const;
    int ArgList2DataElementArray( va_list arg_list );
    int DataElementArray2ArgList( va_list arg_list ) const;
    int ArgVec2DataElementArray( const void **data );
    int DataElementArray2ArgVec( void **data ) const;

    static bool_t pdr_packet_data( struct PDR *, Packet * );
    static bool_t pdr_packet_header( struct PDR *, Packet * );

    //Data Members
    uint32_t stream_id;
    int32_t tag;            /* Application/Protocol Level ID */
    Rank src_rank;          /* Null Terminated String */
    char *fmt_str;          /* Null Terminated String */
    
    char *hdr;              /* packed header */
    unsigned int hdr_len;

    char *buf;              /* packed data */
    unsigned int buf_len;

    Rank inlet_rank;
    Rank *dest_arr;
    unsigned int dest_arr_len;
    bool destroy_data;

    std::vector< const DataElement * > data_elements;
    mutable XPlat::Mutex data_sync;
};


}                               /* namespace MRN */
#endif                          /* packet_h */
