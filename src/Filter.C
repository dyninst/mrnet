/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <vector>

#include "Filter.h"
#include "ParentNode.h"
#include "PeerNode.h"

#include "xplat/SharedObject.h"

using namespace std;

namespace MRN
{

/*======================================*
 *    Filter Class Definition        *
 *======================================*/


Filter::Filter(FilterInfoPtr filterInfo, unsigned short iid, Stream * strm, filter_type_t type)
    : _id(iid), _filter_state(NULL), _strm(strm), _params(Packet::NullPacket), _type(type)
{
    FilterInfo& finfo = (*(filterInfo.get()))[iid];
    _filter_func =
        (void (*)(const vector<PacketPtr>&, vector<PacketPtr>&, vector<PacketPtr>&, 
                  void **, PacketPtr&, const TopologyLocalInfo& ))
        finfo.filter_func;

    _get_state_func = ( PacketPtr (*)( void **, int ) ) finfo.state_func;

    _fmt_str = finfo.filter_fmt;
}

Filter::~Filter(void)
{
}

int Filter::push_Packets( vector< PacketPtr >& ipackets,
                          vector< PacketPtr >& opackets,
                          vector< PacketPtr >& opackets_reverse,
                          const TopologyLocalInfo& topol_info )
{
    std::vector< PacketPtr >::iterator iter;

    mrn_dbg_func_begin();

    if( _filter_func == NULL ) {  //do nothing
        opackets = ipackets;
        ipackets.clear();
        mrn_dbg( 3, mrn_printf(FLF, stderr, 
                               "NULL FILTER: returning %" PRIszt" packets\n",
                               opackets.size() ));
        return 0;
    }

    PerfDataMgr* pdm = NULL;
    if( _strm != NULL )
        pdm = _strm->get_PerfData();

    if( pdm != NULL ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_FILTER) ) {
            for( iter = ipackets.begin(); iter != ipackets.end(); iter++ ) {
                if (_type == FILTER_SYNC)
                    (*iter)->start_Timer(PERFDATA_PKT_TIMERS_FILTER_SYNC);
                else
                    (*iter)->start_Timer(PERFDATA_PKT_TIMERS_FILTER_UPDOWN);
                (*iter)->start_Timer (PERFDATA_PKT_TIMERS_FILTER);
            }
        }
    }

    /* For now, we don't allow multiple threads in same filter func at same time.
     * Eventually, the sync filters should be made thread-safe so we can 
     * remove the mutex.
     */
    _mutex.Lock();
    _filter_func( ipackets, opackets, opackets_reverse, 
                  &_filter_state, _params, topol_info );
    _mutex.Unlock();

    if( pdm != NULL ) {
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_FILTER) ||            
            pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_RECV_TO_FILTER) ||
            pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_RECV) ) {

            for( iter = ipackets.begin(); iter != ipackets.end(); iter++ ) {
                PacketPtr p = *iter;
                p->stop_Timer(PERFDATA_PKT_TIMERS_RECV_TO_FILTER);

                if (_type == FILTER_SYNC)
                    p->stop_Timer(PERFDATA_PKT_TIMERS_FILTER_SYNC);
                else
                    p->stop_Timer(PERFDATA_PKT_TIMERS_FILTER_UPDOWN);

                p->stop_Timer (PERFDATA_PKT_TIMERS_FILTER);
                pdm->add_PacketTimers(p);
                p->reset_Timers();
            }

        }
        if( pdm->is_Enabled(PERFDATA_MET_ELAPSED_SEC, PERFDATA_CTX_PKT_FILTER_TO_SEND))
        {
            for( iter = opackets.begin(); iter != opackets.end(); iter++ )
            {
                (*iter)->start_Timer(PERFDATA_PKT_TIMERS_FILTER_TO_SEND);
            }
        }
    }
    ipackets.clear();
    
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

int Filter::load_FilterFunc( FilterInfoPtr filterInfo, unsigned short iid, const char *iso_file, const char *ifunc_name )
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
        mrn_dbg( 1, mrn_printf(FLF, stderr, "XPlat::SharedObject::Load(%s): %s\n",
                 iso_file, XPlat::SharedObject::GetErrorString() ));
        return -1;
    }

    // find where the filter function is loaded
    filter_func_ptr = (void(*)())so_handle->GetSymbol( ifunc_name );
    if( filter_func_ptr == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "XPlat::SharedObject::GetSymbol(%s): %s\n",
                               ifunc_name, XPlat::SharedObject::GetErrorString() ));

        delete so_handle;
        return -1;
    }

    // find where the filter state function is loaded
    state_func_ptr = (void(*)())so_handle->GetSymbol( state_func_name.c_str() );
    // we don't test the ptr because it doesn't have to exist

    // find where the filter state format string is loaded
    fmt_str_ptr = ( const char ** )so_handle->GetSymbol( func_fmt_str.c_str() );
    if( fmt_str_ptr == NULL ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr,
                               "XPlat::SharedObject::GetSymbol(%s): %s\n",
                               func_fmt_str.c_str(),
                               XPlat::SharedObject::GetErrorString()));
        delete so_handle;
        return -1;
    }

    if( ! register_Filter( filterInfo, iid, filter_func_ptr, state_func_ptr, *fmt_str_ptr ) )
        return -1;

    return 0;
}

bool Filter::register_Filter(FilterInfoPtr filterInfo,
                              unsigned short iid,
                              void (*ifilter_func)(),
                              void (*istate_func)(),
                              const char *ifmt )
{
    mrn_dbg_func_begin();

    FilterInfo finfo( ifilter_func, istate_func, ifmt );
    if( filterInfo->find( iid ) == filterInfo->end() ) {
        (*(filterInfo.get()))[iid] = finfo;
        return true;
    }
    mrn_dbg( 1, mrn_printf(FLF, stderr,
                           "Filter id %hu already registered\n", iid));
    return false;
}

} // namespace MRN
