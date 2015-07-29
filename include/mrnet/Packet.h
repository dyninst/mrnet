/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__packet_h)
#define __packet_h 1

#include "mrnet/Types.h"

#include <cstdarg>
#include <set>
#include <vector>

#include <boost/shared_ptr.hpp>
#include "mrnet/DataElement.h"
#include "mrnet/Error.h"
#include "xplat/Mutex.h"

struct PDR;
namespace MRN
{
class Timer;
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
    int unpack( va_list iarg_list, const char* ifmt, bool );
    const DataElement* operator[]( unsigned int i ) const;

    int get_Tag(void) const;
    void set_Tag( int itag ) { tag = itag; }

    unsigned int get_StreamId(void) const;
    void set_StreamId( unsigned int istream_id ) { stream_id = istream_id; }

    const char* get_FormatString(void) const;
    Rank get_InletNodeRank(void) const;
    Rank get_SourceRank(void) const;

    bool get_Destinations( unsigned& num_dest, Rank** dests );
    bool set_Destinations( const Rank* bes, unsigned int num_bes );

    bool operator==( const Packet & ) const;
    bool operator!=( const Packet & ) const;

    void set_DestroyData( bool b );

    // END MRNET API


    // Only use these if you **really** know what you are doing!!
    const char *get_Buffer(void) const;
    uint64_t get_BufferLen(void) const;
    const char *get_Header(void) const;
    unsigned int get_HeaderLen(void) const;

    // Starts and stops a timer for a specific context
    void start_Timer (perfdata_pkt_timers_t context);
    void stop_Timer (perfdata_pkt_timers_t context);
    void reset_Timers ();

    // Sets a timer for a specific context to t 
    // (used in cases where packet class not yet created EX: recv)
    void set_Timer (perfdata_pkt_timers_t context, Timer t);

    // Get the eleased time in the context timer
    double get_ElapsedTime (perfdata_pkt_timers_t context);
    void set_IncomingPktCount(int size);
    void set_OutgoingPktCount(int size);

    ~Packet();

 private:

    Packet( Rank isrc, unsigned int istream_id, int itag, 
            const char *ifmt, va_list idata );
    Packet( Rank isrc, unsigned int istream_id, int itag, 
            const void **idata, const char *ifmt );
    Packet( unsigned int ihdr_len, char *ihdr, 
            uint64_t ibuf_len, char *ibuf, 
            Rank iinlet_rank );
    void encode_pdr_header(void);
    void encode_pdr_data(void);
    void decode_pdr_header(void) const;
    void decode_pdr_data(void) const;

    void set_SourceRank( Rank r ) { src_rank = r; }

    size_t get_NumDataElements(void) const;
    const DataElement * get_DataElement( unsigned int i ) const;

    int ExtractVaList( const char *fmt, va_list arg_list ) const;
    int ArgList2DataElementArray( va_list arg_list );
    int DataElementArray2ArgList( va_list arg_list ) const;
    int ArgVec2DataElementArray( const void **data );
    int DataElementArray2ArgVec( void **data ) const;

    static int pdr_packet_data( struct PDR *, Packet * );
    static int pdr_packet_header( struct PDR *, Packet * );

    //Data Members
    uint32_t stream_id;
    int32_t tag;            /* Application/Protocol Level ID */
    Rank src_rank;          /* Null Terminated String */
    char *fmt_str;          /* Null Terminated String */
    
    char *hdr;              /* packed header */
    unsigned int hdr_len;

    char *buf;              /* packed data */
    uint64_t buf_len;

    Rank inlet_rank;
    Rank *dest_arr;
    uint64_t dest_arr_len;
    bool destroy_data;

    std::vector< const DataElement * > data_elements;
    mutable XPlat::Mutex data_sync;

    Timer * _perf_data_timer;
    int _in_packet_count;
    int _out_packet_count;
    mutable bool _decoded;
    mutable char _byteorder;
};


}                               /* namespace MRN */
#endif                          /* packet_h */
