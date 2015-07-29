/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( __event_h )
#define __event_h  1

#include <algorithm>
#include <list>
#include <map>
#include <string>

#include "mrnet/Types.h"
#include "xplat/Monitor.h"
#include "xplat/Mutex.h"


namespace MRN {

typedef int EventClass;
typedef int EventType;

class Stream;  
class EventMgr;

class EventData {  
    // strictly abstract
};

class Event {
 protected:
    EventClass _class;
    EventType _typ;
    EventData* _data;

 public:
    static const EventClass EVENT_CLASS_ALL;
    static const EventClass DATA_EVENT;
    static const EventClass TOPOLOGY_EVENT;
    static const EventClass ERROR_EVENT;

    static EventType EVENT_TYPE_ALL;

    Event( EventClass iclass, EventType ityp, EventData* idata )
         : _class(iclass), _typ(ityp), _data(idata) 
    {}

    ~Event()
    {
        _data = NULL;
    }

    inline EventClass get_Class() const { return _class; }
    inline EventType  get_Type() const { return _typ; }
    inline EventData* get_Data() const { return _data; }
};

class DataEvent: public Event {
 public:
    
    static EventType DATA_AVAILABLE;
    static EventType min_Type() { return DATA_AVAILABLE; }
    static EventType max_Type() { return DATA_AVAILABLE; }

    class DataEventData : public EventData {
     private:
        Stream* _strm;
  
     public:
        DataEventData( Stream* istrm ) 
            : _strm(istrm)
        {}

        ~DataEventData()
        {
            _strm = NULL;
        }
	 
        Stream* get_Stream() { return _strm; }
    };

    DataEvent( EventType etyp, DataEventData* edata )
        : Event( DATA_EVENT, etyp, edata )
    {}

    ~DataEvent() {}
};

class ErrorEvent: public Event {
 public:
    
    static EventType ERROR_INTERNAL;
    static EventType ERROR_SYSTEM;
    static EventType ERROR_USAGE;
    static EventType min_Type() { return ERROR_INTERNAL; }
    static EventType max_Type() { return ERROR_USAGE; }

    class ErrorEventData : public EventData {
     private:
        Rank _rank;
        std::string _msg;
  
     public:
 
        ErrorEventData( Rank irank, const std::string& imsg ) 
            : _rank(irank), _msg(imsg)
        {}

        ~ErrorEventData() {}
	 
        Rank get_Rank() const { return _rank; }
        std::string get_Message() const { return _msg; }
    };

    ErrorEvent( EventType etyp, ErrorEventData* edata )
        : Event( ERROR_EVENT, etyp, edata )
    {}

    ~ErrorEvent() {}
};

class TopologyEvent: public Event {
 public:

    static EventType TOPOL_ADD_BE;
    static EventType TOPOL_ADD_CP;
    static EventType TOPOL_REMOVE_NODE;
    static EventType TOPOL_CHANGE_PARENT;
    static EventType min_Type() { return TOPOL_ADD_BE; }
    static EventType max_Type() { return TOPOL_CHANGE_PARENT; }
    
    class TopolEventData : public EventData {
     private:
        Rank _rank;
        Rank _parent_rank;
  
     public:
 
        TopolEventData( Rank irank, Rank iprank ) 
            : _rank(irank), _parent_rank(iprank)
        {}

        ~TopolEventData() {}
 
        Rank get_Rank() const { return _rank; }
        Rank get_Parent_Rank() const { return _parent_rank; }
    };

    TopologyEvent( EventType etyp, TopolEventData* edata )
        : Event( TOPOLOGY_EVENT, etyp, edata )
    {}

    ~TopologyEvent() {}
};
 
typedef void (*evt_cb_func)( Event*, void* );

class evt_cb_info {
 public:
    evt_cb_func func;
    void* data;
    bool onetime;

    evt_cb_info( evt_cb_func ifunc, void* idata, bool once )
        : func(ifunc), data(idata), onetime(once)
    {}

    evt_cb_info( const evt_cb_info& info )
        : func(info.func), data(info.data), onetime(info.onetime)
    {}

    ~evt_cb_info(void) {}
};

typedef std::list< evt_cb_info > evt_cb_list;
typedef std::map< EventType, evt_cb_list > evt_typ_cb_map;

class EventMgr {
 private:
    std::list< Event* > _evts;
    std::map< EventClass, evt_typ_cb_map > _cbs;
    mutable XPlat::Monitor data_sync;

    bool execute_Callbacks( Event* ievt );
    bool remove_CallbackFunc( evt_cb_func icb,
                              evt_cb_list& ifuncs );
    void reset_Callbacks();

 public:
    
    EventMgr();
    ~EventMgr();

    // event management
    bool have_Events() const;
    unsigned int get_NumEvents() const;

    bool add_Event( Event* );
    Event* get_NextEvent();
    void clear_Events();

    // callback management
    bool register_Callback( EventClass iclass, EventType ityp, 
                            evt_cb_func ifunc, void* icb_data,
                            bool once );
    bool remove_Callbacks( EventClass iclass, EventType ityp );
    bool remove_Callback( evt_cb_func ifunc, EventClass iclass, EventType itype );
};

void PipeNotifyCallbackFn( Event*, void* );

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



} /* namespace MRN */

#endif /* __event_h */
