/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "utils.h"
#include "mrnet/Event.h"
#include "xplat/SocketUtils.h"

using namespace std;

namespace MRN {

// generic write-to-notify-pipe callback
void PipeNotifyCallbackFn( CBClass icbcl,CBType icbt,EventCB* icl )
{
	Event* e = (Event*)icl;
	EventPipe* ep = (EventPipe*)e->usr_data;

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
map< EventType, pair< cb_func, void* > > Event::evt_cb_fns;
list< Event* > Event::events;

void Event::new_Event( EventType ityp, EventId iid, Rank irank, string idesc )
{
 
	map< EventType, pair< cb_func, void* > >::iterator iter = evt_cb_fns.find(ityp);
	if( iter != evt_cb_fns.end() ) {
		Event* e = new Event(ityp, iid, irank, idesc,iter->second.second);
		if( e != NULL )
			events.push_back( e );
		iter->second.first( PIPE_NOTIFY,PIPE_DEFAULT,(EventCB*)e );
      }

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

bool Event::register_EventCallback( EventType ityp, cb_func ifn, void* iuser_data )
{

	map< EventType, pair< cb_func, void* > >::iterator iter = evt_cb_fns.find(ityp);
	if( iter == evt_cb_fns.end() ) {
		evt_cb_fns[ityp] = make_pair( ifn, iuser_data );
		return true;
	}
	return false;
}

bool Event::remove_EventCallback( EventType ityp )
{
     map< EventType, pair< cb_func, void* > >::iterator iter = evt_cb_fns.find(ityp);
    if( iter != evt_cb_fns.end() ) {
        evt_cb_fns.erase( iter );
        return true;
    }
    return false;
}


 map< CBClass,map<CBType,list< cb_func > >  > Callback::cbs;
 list<Callback* > Callback::cbacks;
 map<CBClass ,list<CBType> > Callback::all_cb_cl_typ;
 
bool Callback::registerCallback(CBClass icbcl, cb_func icb,CBType icbt)
{
 
   std::map< CBClass,map<CBType,list<cb_func > > >::iterator i= cbs.find(icbcl);
   if(i==cbs.end())
   {
 
         std::map<CBType,list<cb_func> >ins_map;
         list <cb_func> ins_list;
         ins_list.push_back(icb);
 
 
         if(icbt==ALL_EVENT)
         {
                 //Insert CB for first time for all event types 
 
                 map<CBClass,list<CBType> >::iterator i_all_map = all_cb_cl_typ.find(icbcl);
                 list<CBType> &all_list = i_all_map->second;
                 list<CBType> ::iterator i_all_list;
                 for(i_all_list = all_list.begin(); i_all_list != all_list.end();i_all_list++)
                 {
                         ins_map[(*i_all_list)]= ins_list;
                 }
 
                 cbs[icbcl]=ins_map;
 
         }
         else
         {
 
         //Inserting CB  for the first time for particular class,type
         ins_map[icbt]=ins_list;
         cbs[icbcl]=ins_map;
         }
 
   }
   else
   {
         if(icbt==ALL_EVENT)
         {
                 //Insert CB for all event types (not first time)
 
                 std::map<CBType,list<cb_func> >&ins_map = i->second;
 
                 map<CBClass,list<CBType> >::iterator i_all_map = all_cb_cl_typ.find(icbcl);
                 list<CBType> &all_list = i_all_map->second;
                 list<CBType> ::iterator i_all_list;
                 for(i_all_list = all_list.begin(); i_all_list != all_list.end();i_all_list++)
                 {
 
                         std::map<CBType,list<cb_func> >::iterator ii = ins_map.find((*i_all_list));
                         if(ii==ins_map.end())
                         {
                                 //Insert  CB first time for particular type
                                 list <cb_func> ins_list;
                                 ins_list.push_back(icb);
                                 ins_map[(*i_all_list)]= ins_list;
                         }
                         else
                         {
 
                                 list <cb_func> &ins_list = ii->second;
                                 ins_list.push_back(icb);
                                 ins_map[(*i_all_list)]=ins_list;
                         }
                 }
                 cbs[icbcl]=ins_map;
         }
         else
         {
 
                 std::map<CBType,list<cb_func> >&ins_map = i->second;
                 std::map<CBType,list<cb_func> >::iterator ii = ins_map.find(icbt);
 
 
                 if(ii==ins_map.end())
                 {
                         //Inserting CB first time for particular type not class
 
                         list <cb_func> ins_list;
                         ins_list.push_back(icb);
                         ins_map[icbt]=ins_list;
                         cbs[icbcl]=ins_map;
 
 
                 }
                 else
                 {
                         //Inserting CB for particular class,type( not first time)
                         list <cb_func> &ins_list = ii->second;
                         ins_list.push_back(icb);
                         ins_map[icbt]=ins_list;
                         cbs[icbcl]=ins_map;
 
 
                 }
           }
 
 }
 

 
  return true;
}
 
 
 
bool Callback::removeCallback(CBClass icbcl,CBType icbt)
{
 
         bool ret = removeCallback(icbcl,NULL,icbt);
 
         return ret;
 
}
bool Callback::removeCallback(CBClass icbcl,cb_func icb,CBType icbt)
{
 
         std::map< CBClass,map<CBType, list<cb_func> > >::iterator i= cbs.find(icbcl);
         if(i==cbs.end())
         {
                 return false;
         }
         if(icbt==ALL_EVENT)
         {
                 //Remove for all event type
                 if(icb==NULL)
                 {
                         //Remove all functions
                         cbs.erase(i);
                         return true;
                 }
                 else
                 {
 
                 //Remove only function = icb 
                         int rm_flag=0;
 
                         map<CBType,list<cb_func> > &rm_map = i->second;
                         map<CBType,list<cb_func> >::iterator ii;
                         for(ii = rm_map.begin(); ii!=rm_map.end();ii++)
                         {
                                 bool retval= removeCallback_list(icb,ii);
                                 if(retval)
                                         rm_flag++;
                         }
 
                         if(rm_flag==0)
                                 return false;
                         else
                                 return true;
                 }
         }
         else
         {
                 //Remove for only icbt event type
 
                 map<CBType,list<cb_func> > &rm_map = i->second;
                 map<CBType,list<cb_func> >::iterator ii = rm_map.find(icbt);
                 if(ii==rm_map.end())
                 {
                         return false;
                 }
 
                 if(icb==NULL)
                 {
                         //Remove all functions
 
                         rm_map.erase(ii);
                         return true;
                 }
                 else
                 {
                         //Remove only function = icb
 
                         bool retval= removeCallback_list(icb,ii);
                         return retval;
                 }
 
 
         }
 
 
 
}
 
bool Callback::removeCallback_list(cb_func icb,map<CBType,list<cb_func> >::iterator ii)
{
 
                         list<cb_func> &rm_list = ii->second;
                         list<cb_func>::iterator rm_li;
                         for(rm_li=rm_list.begin(); rm_li != rm_list.end() ; rm_li++)
                         {
                                 if( (*rm_li) == icb)
                                 {
                                         rm_list.erase(rm_li);
                                         return true;
                                 }
                         }
                         return false;
 
 
}
 
bool Callback::executeCB(CBClass icbcl,CBType icbt,EventCB* icl_typ )
{
 
  Callback* cb = new Callback(icbcl);
  if(cb !=NULL)
         cbacks.push_back( cb );
 
 
 map< CBClass,map< CBType, list<cb_func> > >::iterator i =cbs.find(icbcl);
 
 if(i==cbs.end())
 {
         return false;
 }
 else
 {
         map<CBType,list<cb_func> > &ex_map=i->second;
         map<CBType,list<cb_func> >::iterator ii = ex_map.find(icbt);
 
         if(ii==i->second.end())
         {
                 return false;
         }
         else
         {
                 list<cb_func> &ex_list = ii->second;
                 list<cb_func>::iterator li;
                 for(li=ex_list.begin() ;li!=ex_list.end() ; li++)
                 {
                         (*li)(icbcl,icbt,icl_typ);
                 }
         }
 
 
 }
 
 
return true;
 
}
 
unsigned int Callback::get_NumCB()
{
         return cbacks.size();
}



} // namespace MRN
