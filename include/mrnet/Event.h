/****************************************************************************
 *  Copyright 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
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

typedef void (*EventCallbackFunction)( EventType, EventId, Rank, const char*, void* );

void PipeNotifyCallbackFn( EventType, EventId, Rank, const char*, void* );

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


class Event {
 private:
    static std::map< EventType, std::pair< EventCallbackFunction, void* > > evt_cb_fns;
    static std::list< Event* > events;

    EventType typ;
    EventId id;
    Rank rank;
    std::string desc;

    //explicitly disallow creation without "new_Event()"
    Event( EventType ityp, EventId iid, Rank irank, std::string& idesc )
        : typ(ityp), id(iid), rank(irank), desc(idesc) {} 

 public:
    ~Event() {}

    static void new_Event( EventType ityp, EventId iid, Rank irank, std::string idesc="" );
    static bool have_Event();
    static unsigned int get_NumEvents();
    static Event* get_NextEvent(void);
    static void clear_Events(void);

    static bool register_EventCallback( EventType, EventCallbackFunction, void* );
    static bool remove_EventCallback( EventType );

    inline EventType get_Type() const { return typ; }
    inline EventId get_Id() const { return id; }
    inline Rank get_Rank() const { return rank; }
    inline const std::string& get_Description() const { return desc; } 
};

} /* namespace MRN */

#endif /* __event_h */
