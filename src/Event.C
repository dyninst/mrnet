/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils.h"
#include "mrnet/Event.h"
#include "xplat/SocketUtils.h"

using namespace std;

namespace MRN {

// generic write-to-notify-pipe callback
void PipeNotifyCallbackFn( Event*, void* idata )
{
    EventPipe* ep = (EventPipe*)idata;
    ep->signal();
}

EventPipe::EventPipe()
    : _has_data(false)
{
    set_ReadFd( -1 );
    set_WriteFd( -1 );
#if !defined(os_windows)
    int pipeFDs[2];
    int ret = pipe(pipeFDs);
    if( ret == 0 ) {
        _sync.Lock();
        set_ReadFd( pipeFDs[0] );
        set_WriteFd( pipeFDs[1] );
        _sync.Unlock();
    }
    else
        mrn_dbg(1, mrn_printf(FLF, stderr, "pipe() failed - %s", strerror(errno)));
#endif
}

EventPipe::~EventPipe()
{
#if !defined(os_windows)
    _sync.Lock();
    if( _pipe_fds[0] != -1 ) {
        XPlat::SocketUtils::Close( _pipe_fds[0] );
        set_ReadFd( -1 );
    }
    if( _pipe_fds[1] != -1 ) {
        XPlat::SocketUtils::Close( _pipe_fds[1] );
        set_WriteFd( -1 );
    }
    _sync.Unlock();
#endif
}

void EventPipe::signal(void)
{
#if !defined(os_windows)
    _sync.Lock();
    if(( get_WriteFd() == -1 ) || _has_data ) {
        _sync.Unlock();
        return;
    }
    _sync.Unlock();

    mrn_dbg(5, mrn_printf(FLF, stderr, "writing pipefd\n" ));
    char c = '!';
    ssize_t ret = write( get_WriteFd(), &c, sizeof(char) );
    if( ret == -1 )
        mrn_dbg(1, mrn_printf(FLF, stderr, "write() failed - %s", strerror(errno)));
    else {
        _sync.Lock();
        _has_data = true;;
        _sync.Unlock();
    }
#endif
}

void EventPipe::clear(void)
{
#if !defined(os_windows)
    _sync.Lock();
    if(( get_ReadFd() == -1 ) || ( ! _has_data )) {
        _sync.Unlock();
        return;
    }
    _sync.Unlock();

    mrn_dbg(5, mrn_printf(FLF, stderr, "clearing pipefd\n" ));
    char c;
    ssize_t ret = read( get_ReadFd(), &c, sizeof(char) );
    if( ret == -1 )
        mrn_dbg(1, mrn_printf(FLF, stderr, "read() failed - %s", strerror(errno)));
    else {
        _sync.Lock();
        _has_data = false;
        _sync.Unlock();
    }
#endif
}

// static Event members
const EventClass Event::EVENT_CLASS_ALL = 0;
const EventClass Event::DATA_EVENT      = 1;
const EventClass Event::TOPOLOGY_EVENT  = 2;
const EventClass Event::ERROR_EVENT     = 3;

EventType Event::EVENT_TYPE_ALL = -1;

EventType DataEvent::DATA_AVAILABLE = 0;

EventType ErrorEvent::ERROR_INTERNAL = 0;
EventType ErrorEvent::ERROR_SYSTEM   = 1;
EventType ErrorEvent::ERROR_USAGE    = 2;

EventType TopologyEvent::TOPOL_ADD_BE        = 0;
EventType TopologyEvent::TOPOL_ADD_CP        = 1;
EventType TopologyEvent::TOPOL_REMOVE_NODE   = 2;
EventType TopologyEvent::TOPOL_CHANGE_PARENT = 3;

EventMgr::EventMgr()
{
    reset_Callbacks();
}

EventMgr::~EventMgr()
{
    clear_Events();

    data_sync.Lock();
    _cbs.clear();
    data_sync.Unlock();
}

void EventMgr::reset_Callbacks()
{
    data_sync.Lock();

    // initialize the event class/type callback maps
    EventType min, max;
    evt_cb_list empty_list;
    int u;

    min = DataEvent::min_Type();
    max = DataEvent::max_Type();
    evt_typ_cb_map& data_map = _cbs[ Event::DATA_EVENT ];
    for( u = min; u <= max; u++ )
        data_map[ u ] = empty_list;

    min = ErrorEvent::min_Type();
    max = ErrorEvent::max_Type();
    evt_typ_cb_map& error_map = _cbs[ Event::ERROR_EVENT ];
    for( u = min; u <= max; u++ )
        error_map[ u ] = empty_list;

    min = TopologyEvent::min_Type();
    max = TopologyEvent::max_Type();
    evt_typ_cb_map& topol_map = _cbs[ Event::TOPOLOGY_EVENT ];
    for( u = min; u <= max; u++ )
        topol_map[ u ] = empty_list;

    data_sync.Unlock();
}

bool EventMgr::have_Events() const
{
    bool rc;
    data_sync.Lock();
    rc = ! _evts.empty();
    data_sync.Unlock();
    return rc;
}

unsigned int EventMgr::get_NumEvents() const
{
    unsigned int rc;
    data_sync.Lock();
    rc = (unsigned int)_evts.size();
    data_sync.Unlock();
    return rc;
}
    
bool EventMgr::add_Event( Event* ievt )
{
    if( ievt != NULL ) {
        data_sync.Lock();
        _evts.push_back( ievt );
        execute_Callbacks( ievt );
        data_sync.Unlock();
        return true;
    }
    return false;
}

Event* EventMgr::get_NextEvent()
{
    data_sync.Lock();
    if( _evts.size() ) {
        Event* ret = _evts.front();
        _evts.pop_front();
        data_sync.Unlock();
        return ret;
    }
    data_sync.Unlock();
    return NULL;
}

void EventMgr::clear_Events()
{
    data_sync.Lock();

    std::list< Event* >::iterator eiter = _evts.begin();
    for( ; eiter != _evts.end(); eiter++ ) {
        if( *eiter != NULL ) {
            Event* evt = *eiter;
            EventData* evt_data = evt->get_Data();
            if( evt_data != NULL ) {
                EventClass c = evt->get_Class();
                switch( c ) {
                case Event::DATA_EVENT:
                    delete (DataEvent::DataEventData*) evt_data;
                    break;
                case Event::TOPOLOGY_EVENT:
                    delete (TopologyEvent::TopolEventData*) evt_data;
                    break;
                case Event::ERROR_EVENT:
                    delete (ErrorEvent::ErrorEventData*) evt_data;
                    break;
                }
            }
            delete evt;
        }
    }

    _evts.clear();

    data_sync.Unlock();
}

bool EventMgr::register_Callback( EventClass iclass, EventType ityp,
                                  evt_cb_func ifunc, void* idata,
                                  bool once )
{
    if( ifunc == (evt_cb_func)NULL )
        return false;
    
    if( once && (ityp == Event::EVENT_TYPE_ALL) ) {
        mrn_dbg( 1, mrn_printf(FLF, stderr, 
           "one-time function must be registered with specific event type\n") );
        return false;
    }

    evt_cb_info cb_info( ifunc, idata, once );

    data_sync.Lock();

    std::map< EventClass, evt_typ_cb_map >::iterator i = _cbs.find( iclass );
    if( i == _cbs.end() ) {
        data_sync.Unlock();
        return false;
    }
 
    evt_typ_cb_map& ins_map = i->second;
    evt_typ_cb_map::iterator mapi;
    if( ityp == Event::EVENT_TYPE_ALL ) {
        mapi = ins_map.begin();
        for( ; mapi != ins_map.end(); mapi++ ) {
            evt_cb_list& elist = mapi->second;
            elist.push_back( cb_info );
        }
    }
    else {
        mapi = ins_map.find(ityp);                
        if( mapi == ins_map.end() ) {
            data_sync.Unlock();
            return false;
        }

        evt_cb_list& ins_list = mapi->second;
        ins_list.push_back( cb_info );
    }

    data_sync.Unlock();
    return true;
}
  
bool EventMgr::remove_Callbacks( EventClass iclass, EventType ityp )
{
    if( iclass == Event::EVENT_CLASS_ALL ) {
        if( ityp == Event::EVENT_TYPE_ALL ) {
            reset_Callbacks();
            return true;
        }
        else {
            // invalid use
            return false;
        }
    }

    return remove_Callback( (evt_cb_func)NULL, iclass, ityp );
}

bool EventMgr::remove_Callback( evt_cb_func ifunc, EventClass iclass, EventType ityp )
{
    bool rc;

    data_sync.Lock();

    std::map< EventClass, evt_typ_cb_map >::iterator i;
    i = _cbs.find( iclass );
    if( i == _cbs.end() ) {
        data_sync.Unlock();
        return false;
    }

    evt_typ_cb_map& rm_map = i->second;
    evt_typ_cb_map::iterator mapi;
    if( ityp == Event::EVENT_TYPE_ALL ) {
        //Remove for all event type
        if( ifunc == (evt_cb_func)NULL ) {
            //Remove all functions
            for( mapi = rm_map.begin(); mapi != rm_map.end(); mapi++ )
                mapi->second.clear();
            data_sync.Unlock();
            return true;
        }
        else {
            //Remove only ifunc 
            bool rm_flag = false;
            for( mapi = rm_map.begin(); mapi != rm_map.end(); mapi++ ) {
                rc = remove_CallbackFunc( ifunc, mapi->second );
                if( rc )
                    rm_flag = true;
            }
            data_sync.Unlock();
            return rm_flag;
        }
    }
    else {
        //Remove for only ityp event type
        mapi = rm_map.find(ityp);
        if( mapi == rm_map.end() ) {
            data_sync.Unlock();
            return false;
        }
         
        if( ifunc == (evt_cb_func)NULL ) {
            //Remove all functions
            mapi->second.clear();
            data_sync.Unlock();
            return true;
        }
        else {
            //Remove only ifunc
            rc = remove_CallbackFunc( ifunc, mapi->second );
            data_sync.Unlock();
            return rc;
        }
    }
    // shouldn't get here  
    data_sync.Unlock();
    return false;
}
 
bool EventMgr::remove_CallbackFunc( evt_cb_func ifunc,
                                    evt_cb_list& ifuncs )
{
    // no need to lock, done at callers

    evt_cb_list::iterator rm_li = ifuncs.begin();
    for( ; rm_li != ifuncs.end(); rm_li++ ) {
        if( rm_li->func == ifunc ) {
            ifuncs.erase( rm_li );
            return true;
        }
    }
    return false;
}
 
bool EventMgr::execute_Callbacks( Event* ievt )
{
    // no need to lock, done at callers

    EventClass c = ievt->get_Class();
    EventType t = ievt->get_Type();

    map< EventClass, evt_typ_cb_map >::iterator i;
    i = _cbs.find( c );
    if( i == _cbs.end() )
        return false;
    else {
        evt_typ_cb_map& etyp_map = i->second;
        evt_typ_cb_map::iterator mapi = etyp_map.find( t );
        if( mapi == etyp_map.end() )
            return false;
        else {
            evt_cb_list& elist = mapi->second;
            evt_cb_list::iterator li, tmp;
            size_t u, count;
            li = elist.begin();
            count = elist.size();
            for( u = 0; u < count; u++ ) {

                // call function
                (li->func)( ievt, li->data );

                // remove if one-time callback
                if( li->onetime ) {
                    tmp = li++;
                    elist.erase( tmp );
                }
                else
                    li++;
            }        
        }
    }
 
    return true;
}


} // namespace MRN
