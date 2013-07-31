/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include "libi/debug.h"

using namespace LIBI;

debug::debug( bool is_front_end ) {
    _is_front_end = is_front_end;
    _level=ERRORS;
    _identifier=NULL;
    _err_file=NULL;
    _file_path=NULL;
    _output_set=false;
}

debug::~debug() {
    if( _identifier != NULL )
        free( _identifier );
    if( _err_file != NULL && _err_file != stderr )
        free( _err_file );
    if( _file_path != NULL )
        free( _file_path );
}

void debug::getParametersFromEnvironment(){
    char* LIBI_DEBUG = getenv("LIBI_DEBUG");
    if( LIBI_DEBUG != NULL )
        set_level( atoi( LIBI_DEBUG ) );

    char* LIBI_DEBUG_DIR = getenv("LIBI_DEBUG_DIR");
    if( LIBI_DEBUG_DIR != NULL )
        set_dir( LIBI_DEBUG_DIR );
}

void debug::print(int msg_level, const char* func, const int line, char* fmt, ... ){
    va_list argp;

    if( _level >= msg_level ){
        if( _err_file == NULL )
            _err_file = stderr;
        if( _file_path != NULL && !_output_set ){
            FILE * file = fopen( _file_path, "a" );
            _err_file = file;
            _output_set = true;
        }

        timeval t1;
        gettimeofday( &t1, NULL );
        fprintf(_err_file, "%i:%ld.%.6ld:%s:%s(%i) ", getpid(), t1.tv_sec, t1.tv_usec, _identifier, func, line );

        va_start(argp, fmt);
        vfprintf(_err_file, fmt, argp);
        va_end(argp);

        fprintf(_err_file, "\n");
        fflush(_err_file);
    }
}

void debug::set_identifier( char * identifier ){
    _identifier = strdup(identifier);
}

void debug::set_level( int l ){
    _level = l;
}

void debug::set_dir( char* dir ){
    if( dir != NULL && !_output_set ){ //don't change output file once established
        char hostname[256];
        char* base;
        if( _is_front_end )
            base = "LIBIFE";
        else
            base = "LIBI";
        gethostname(hostname, 256);
        _file_path = (char*)malloc(500);
        snprintf(_file_path, 500, "%s/%s.%s.%u", dir, base, hostname, getpid() );
    }
}



