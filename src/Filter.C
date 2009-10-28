/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <vector>

#include "mrnet/MRNet.h"

#include "Filter.h"
#include "ParentNode.h"
#include "PeerNode.h"
#include "utils.h"

#include "xplat/SharedObject.h"

using namespace std;

namespace MRN
{

/*======================================*
 *    Filter Class Definition        *
 *======================================*/
map< unsigned short, Filter::FilterInfo > Filter::Filters;
int FilterCounter::count=0;
static FilterCounter fc;

Filter::Filter(unsigned short iid)
    : _id(iid), _filter_state(NULL), _params(Packet::NullPacket)
{
    FilterInfo& finfo = Filters[iid];
    _filter_func =
        (void (*)(const vector<PacketPtr>&, vector<PacketPtr>&, vector<PacketPtr>&, 
                  void **, PacketPtr&, const TopologyLocalInfo& ))
        finfo.filter_func;

    _get_state_func = ( PacketPtr (*)( void **, int ) ) finfo.state_func;

    _fmt_str = finfo.filter_fmt;
}

Filter::~Filter(  )
{
}

int Filter::push_Packets( vector< PacketPtr >& ipackets,
                          vector< PacketPtr >& opackets,
                          vector< PacketPtr >& opackets_reverse,
                          const TopologyLocalInfo& topol_info )
{
    mrn_dbg_func_begin();

    _mutex.Lock();
    
    if( _filter_func == NULL ) {  //do nothing
        opackets = ipackets;
        ipackets.clear( );
        mrn_dbg( 3, mrn_printf(FLF, stderr, "NULL FILTER: returning %d packets\n",
                               opackets.size( ) ));
        _mutex.Unlock();
        return 0;
    }

    _filter_func( ipackets, opackets, opackets_reverse, &_filter_state, _params, topol_info );
    ipackets.clear( );
    
    _mutex.Unlock();

    mrn_dbg_func_end();
    return 0;
}

PacketPtr Filter::get_FilterState( int istream_id )
{
    mrn_dbg_func_begin();

    if( _get_state_func == NULL ){
        return Packet::NullPacket;
    }

    PacketPtr packet( _get_state_func( &_filter_state, istream_id ) );

    mrn_dbg_func_end();
    return packet;
}

void Filter::set_FilterParams( PacketPtr iparams )
{
   mrn_dbg_func_begin();
   _params = iparams;
}

int Filter::load_FilterFunc( unsigned short iid, const char *iso_file, const char *ifunc_name )
{
    XPlat::SharedObject* so_handle = NULL;
    void (*filter_func_ptr)()=NULL;
    void (*state_func_ptr)()=NULL;
    const char **fmt_str_ptr=NULL;
    string func_fmt_str = ifunc_name;
    func_fmt_str += "_format_string";
    string state_func_name = ifunc_name;
    state_func_name += "_get_state";

    mrn_dbg_func_begin();

    so_handle = XPlat::SharedObject::Load( iso_file );
    if( so_handle == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, "XPlat::SharedObject::Load(\"%s\"): %s\n",
                 iso_file, XPlat::SharedObject::GetErrorString() ));
        return -1;
    }

    //find where the filter function is loaded
    filter_func_ptr = (void(*)())so_handle->GetSymbol( ifunc_name );
    if( filter_func_ptr == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "XPlat::SharedObject::GetSymbol(\"%s\"): %s\n",
                               ifunc_name, XPlat::SharedObject::GetErrorString() ));

        delete so_handle;
        return -1;
    }

    //find where the filter state function is loaded
    //we don't test filter state function ptr because it doesn't have to exist
    state_func_ptr = (void(*)())so_handle->GetSymbol( state_func_name.c_str() );

    //find where the filter state format string is loaded
    fmt_str_ptr = ( const char ** )so_handle->GetSymbol( func_fmt_str.c_str() );
    if( fmt_str_ptr == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "XPlat::SharedObject::GetSymbol(\"%s\"): %s\n",
                               func_fmt_str.c_str(),
                               XPlat::SharedObject::GetErrorString()));
        delete so_handle;
        return -1;
    }

    if( ! register_Filter( iid, filter_func_ptr, state_func_ptr, *fmt_str_ptr ) )
        return -1;

    return 0;
}

bool Filter::register_Filter( unsigned short iid,
                              void (*ifilter_func)(),
                              void (*istate_func)(),
                              const char *ifmt )
{
    mrn_dbg_func_begin();

    FilterInfo finfo( ifilter_func, istate_func, ifmt );
    if( Filters.find( iid ) == Filters.end() ) {
        Filters[iid] = finfo;
        return true;
    }
    mrn_dbg( 1, mrn_printf(FLF, stderr,
                           "Filter id %hu already registered\n", iid));
    return false;
}

} // namespace MRN
