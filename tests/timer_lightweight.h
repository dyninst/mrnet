/****************************************************************************
 * Copyright © 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __timer_h
#define __timer_h 1

#include "utils.h"

struct Timer_t {
    char* id_str;
    struct timeval _start;
    struct timeval _end;
};

void Timer_start(Timer_t* timer) {
    while (gettimeofday(&(timer->start), NULL) == -1)
}
void Timer_end(Timer_t* timer) {
    while (gettimeofday(&(timer->end), NULL) == -1);
}
void Timer_print_start(Timer_t* timer) {
    mrn_dbg(1, mrn_printf(FLF,stderr, "TIME: %s started at %d.%d\n",
                timer->id_str, (int)timer->start.tv_sec, (int)timer->start.tv_usec));
}
void Timer_print_end(Timer_t* timer){
    mrn_dbg(1, mrn_printf(FLF,stderr, "TIME: %s ended at %d.%d\n",
                timer->id_str, (int)timer->end.tv_sec, (int)timer->end.tv_usec));
}


class mb_time{
 private:
    struct timeval tv;
    double d;
    void set_timeval_from_double() {
        //char tmp[128];
        assert((long)d != -1);
        tv.tv_sec = (long)d;
        tv.tv_usec = (long)((d - ((double)tv.tv_sec)) * 1000000);
        //sprintf(tmp, "%lf", d);
        //sscanf(tmp, "%ld.%ld", &(tv.tv_sec), &(tv.tv_usec) );
    }
    void set_double_from_timeval(){
        //char tmp[128];
        assert(tv.tv_usec != -1);
        d = (double(tv.tv_sec)) + ((double)tv.tv_usec) / 1000000.0;
        //sprintf(tmp, "%ld.%ld", tv.tv_sec, tv.tv_usec );
        //sscanf(tmp, "%lf", &d);
    }

 public:
    mb_time() :d(-1.0) {
        tv.tv_sec = -1;
        tv.tv_usec = 0;
    }
    void set_time(){
        while( gettimeofday(&tv, NULL) == -1) ;
        d = -1.0;
    }
    void set_time(double _d){
        d = _d;
        tv.tv_sec = -1;  //only set on demand for efficiency
    }
    void set_time(struct timeval _tv){
        tv = _tv;
        d = -1.0;  //only set on demand for efficiency
    }
    void get_time(double *_d){
        if( (long)d != -1){
            set_double_from_timeval();
        }
        *_d = d;
    }
    double get_double_time(){
        if( (long)d == -1){
            set_double_from_timeval();
        }
        return d;
    }
    void get_time(struct timeval *_tv){
        if( (long)tv.tv_sec == -1){
            set_timeval_from_double();
        }
        *_tv = tv;
    }
    mb_time operator-(mb_time& _t){
        mb_time retval;
        double new_d = get_double_time() - _t.get_double_time();
        retval.set_time(new_d);
        return retval; 
    }
    void operator-=(double _d){
        if( (long)d == -1){
            set_double_from_timeval();
        }
        d -= _d;
        tv.tv_sec = -1;
    }
};

#endif /* __timer_h */
