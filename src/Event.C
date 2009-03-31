/****************************************************************************
 *  Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils.h"
#include "mrnet/Event.h"

using namespace std;

namespace MRN {

// generic write-to-notify-pipe callback
void PipeNotifyCallbackFn( EventType, EventId, Rank, const char*, void* user_data )
{
    EventPipe* ep = (EventPipe*)user_data;
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
        mrn_printf(FLF, stderr, "pipe() failed - %s", strerror(errno));
#endif
}

EventPipe::~EventPipe()
{
#if !defined(os_windows)
    _sync.Lock();
    if( _pipe_fds[0] != -1 ) {
        close( _pipe_fds[0] );
        set_ReadFd( -1 );
    }
    if( _pipe_fds[1] != -1 ) {
        close( _pipe_fds[1] );
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
    int ret = write( get_WriteFd(), &c, sizeof(char) );
    if( ret == -1 )
        mrn_printf(FLF, stderr, "write() failed - %s", strerror(errno));
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
    int ret = read( get_ReadFd(), &c, sizeof(char) );
    if( ret == -1 )
        mrn_printf(FLF, stderr, "read() failed - %s", strerror(errno));
    else {
        _sync.Lock();
        _has_data = false;
        _sync.Unlock();
    }
#endif
}

// static Event data
map< EventType, pair< EventCallbackFunction, void* > > Event::evt_cb_fns;
list< Event* > Event::events;

void Event::new_Event( EventType ityp, EventId iid, Rank irank, string idesc )
{
    Event* e = new Event(ityp, iid, irank, idesc);
    if( e != NULL )
        events.push_back( e );
 
    map< EventType, pair< EventCallbackFunction, void* > >::iterator iter = evt_cb_fns.find(ityp);
    if( iter != evt_cb_fns.end() )
        iter->second.first( ityp, iid, irank, idesc.c_str(), iter->second.second );
}

bool Event::have_Event(void)
{
    if( events.size() )
        return true;
    return false;
}

Event* Event::get_NextEvent(void)
{
    if( events.size() ) {
        Event* ret = *( events.begin() );
        events.pop_front();
        return ret;
    }
    else
        return NULL;
}

unsigned int Event::get_NumEvents(void)
{
    return events.size();
}

void Event::clear_Events(void)
{
    events.clear();
}

bool Event::register_EventCallback( EventType ityp, EventCallbackFunction ifn, void* iuser_data )
{
    map< EventType, pair< EventCallbackFunction, void* > >::iterator iter = evt_cb_fns.find(ityp);
    if( iter == evt_cb_fns.end() ) {
        evt_cb_fns[ityp] = make_pair( ifn, iuser_data );
        return true;
    }
    return false;
}

bool Event::remove_EventCallback( EventType ityp )
{
    map< EventType, pair< EventCallbackFunction, void* > >::iterator iter = evt_cb_fns.find(ityp);
    if( iter != evt_cb_fns.end() ) {
        evt_cb_fns.erase( iter );
        return true;
    }
    return false;
}

} // namespace MRN
