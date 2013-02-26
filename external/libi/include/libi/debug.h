/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <stdarg.h>
#include <stdio.h>
#include <cstdlib>
#include <sys/time.h>
#include "libi/libi_api_std.h"

#ifndef LIBI_DEBUG_H
#define	LIBI_DEBUG_H

namespace LIBI {

#define FL  __FUNCTION__,__LINE__

enum debug_level { SILENT=-1, ERRORS=0, FE_LAUNCH_INFO=1, ALL_LAUNCH_INFO=2, INCLUDE_COMMUNICATION_INFO=3 };

class debug {
public:
    debug( bool is_front_end );
    ~debug();
    void getParametersFromEnvironment();
    void set_identifier( char * identifier );
    void set_level( int l );
    void set_dir( char * dir );
    void print( int msg_level, const char* func, const int line, char* fmt, ... );

private:
    bool _is_front_end;
    bool _output_set;
    int _level;
    char * _identifier;
    FILE * _err_file;
    char * _file_path;
};

}

#endif	/* DEBUG_H */

