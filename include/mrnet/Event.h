/****************************************************************************
 *  Copyright 2003-2010 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( __event_h )
#define __event_h  1

#include <string>
#include <list>
#include <map>
#include <algorithm>

#include "mrnet/Types.h"
#include "xplat/Monitor.h"

namespace MRN {

typedef enum {
    ERROR_EVENT=0,
    DATA_EVENT,
    TOPOLOGY_EVENT,
    UNKNOWN_EVENT
} EventType;

typedef int EventId;

typedef enum {
	 ERROR_EVENT_CB=0,
         DATA_EVENT_CB,
         TOPOLOGY_EVENT_CB,
         PIPE_NOTIFY
}CBClass;
  
typedef enum {
	ALL_EVENT=0,
         PIPE_DEFAULT,
         TOPO_ADD_BE,
         TOPO_REMOVE,
         TOPO_ADD_CP,
         TOPO_PARENT_CHANGE
  
}CBType;
  
  
  
 class EventCB {
  
  
};

typedef void (*cb_func) (CBClass,CBType,EventCB*);


class TopoEvent: public EventCB {
 
public:
 
 
	TopoEvent( Rank iprank, Rank irank ) : parent_rank(iprank),node_rank(irank) {}
	~TopoEvent() {}
	 
	Rank parent_rank;
	Rank node_rank;
 
	unsigned int get_Rank() { return node_rank; }
	unsigned int get_Parent_Rank() { return parent_rank; }
 
};
 
 
 

void PipeNotifyCallbackFn( CBClass,CBType, EventCB* );




class EventPipe {
 private:
    int _pipe_fds[2];
    bool _has_data;
    XPlat::Monitor _sync;

    inline int get_WriteFd(void) { return _pipe_fds[1]; }
    inline void set_ReadFd( int i ) { _pipe_fds[0] = i; }
    inline void set_WriteFd( int i) { _pipe_fds[1] = i; }

 public:
    EventPipe();
    ~EventPipe();

    void signal(void);
    void clear(void);
    inline int get_ReadFd(void) { return _pipe_fds[0]; }
};


class Event :public EventCB {
 private:
  static std::map< EventType, std::pair< cb_func, void* > > evt_cb_fns;
  static std::list< Event* > events;

    EventType typ;
    EventId id;
    Rank rank;
    std::string desc;

    //explicitly disallow creation without "new_Event()"
    Event( EventType ityp, EventId iid, Rank irank, std::string& idesc, void* iusr_data )
         : typ(ityp), id(iid), rank(irank), desc(idesc),usr_data(iusr_data)   {}


 public:

    void* usr_data;
    ~Event() {}

    static void new_Event( EventType ityp, EventId iid, Rank irank, std::string idesc="" );
    static bool have_Event();
    static unsigned int get_NumEvents();
    static Event* get_NextEvent(void);
    static void clear_Events(void);

    static bool register_EventCallback( EventType, cb_func, void* );
    static bool remove_EventCallback( EventType );

    inline EventType get_Type() const { return typ; }
    inline EventId get_Id() const { return id; }
    inline Rank get_Rank() const { return rank; }
    inline const std::string& get_Description() const { return desc; } 
};



class Callback
{
public:
 
	Callback(CBClass cbclass): cbcl(cbclass) {}
 
 
	static bool registerCallback(CBClass icbcl,cb_func icb ,CBType icbt= ALL_EVENT );
	static bool removeCallback(CBClass icbcl,CBType icbt=ALL_EVENT);
	static bool removeCallback(CBClass icbcl,cb_func icb,CBType icbt=ALL_EVENT);
	static bool executeCB(CBClass,CBType,EventCB*);
	static unsigned int get_NumCB();
	inline CBClass get_CBClass() const { return cbcl;}
 
	static std::map<CBClass, std::list<CBType> > all_cb_cl_typ;
 
 private:
	static std::map< CBClass,std::map<CBType, std::list<cb_func> > > cbs;
	static std::list< Callback* > cbacks;
 
	static bool removeCallback_list(cb_func icb,std::map<CBType,std::list<cb_func> >::iterator ii);
 
 
 	CBClass cbcl;
 
 };



} /* namespace MRN */

#endif /* __event_h */
