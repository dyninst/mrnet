/**************************************************************************
 * Copyright 2003-2015   Michael J. Brim, Barton P. Miller                *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#ifndef _FILE_UTIL_H_
#define _FILE_UTIL_H_

/* File Handling Utility Functions */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <vector>
#include <fstream>
using namespace std;

#ifndef __FUNCTION__
#define __FUNCTION__ "nofunction"
#endif

#ifdef os_solaris
#define MUNMAP_CAST (char*)
#else
#define MUNMAP_CAST
#endif

inline int stat_file(const char* filename, struct stat* status, int errprint = 1)
{
    if( stat(filename, status) ) {
        int error = errno;
        if( errprint ) {
            fprintf(stderr, "%s - stat(%s) failure: %s\n", 
                    __FUNCTION__, filename, strerror(error));
        }
        return -error;
    }
    return 0;
}

inline int open_file(const char* filename, int flags, mode_t mode, int errprint = 1)
{
    int fd = open(filename, flags, mode);
    if( fd == -1 ) {
        int error = errno;
        if( errprint ) {
            fprintf(stderr, "%s - open(%s, %d) failure: %s\n", 
                    __FUNCTION__, filename, flags, strerror(error));
        }
        return -error;
    }
    return fd;
}

inline char* read_file(const char* filename, size_t& filelen)
{
    ifstream f;
    size_t len;
    char* contents = NULL;
    filelen = 0;
    f.open(filename);
    if(f.is_open()) {

        // get file length
        f.seekg (0, ios::end);
        len = f.tellg();
        if( len == (size_t)-1 ) {
            len = 65536; // is this conservative??
            f.close();
            f.open(filename);
        }
        else {
            f.seekg (0, ios::beg);
            if( (int)(f.tellg()) != 0 ) {
                // some files don't support seek back to beginning (e.g. /proc/*)
                f.close();
                f.open(filename);
            }
        }

        // read entire file
        contents = (char*) malloc(len+512);
        if( contents == NULL )
            fprintf(stderr, "%s[%d] %s - malloc(%zd) failed\n", 
                    __FILE__, __LINE__, __FUNCTION__, len+1);
        else {
            f.read(contents, len+511);
            len = f.gcount();
            contents[len] = '\0';
            filelen = len+1;
        }

        f.close();
    }
    else
        fprintf(stderr, "%s[%d] %s - open(%s) failed\n",
                __FILE__, __LINE__, __FUNCTION__, filename);

    return contents;
}

inline void* map_file( const char* filename, struct stat* status, int errprint=1 )
{
    void* file_buf = NULL;

    if( stat_file(filename, status, errprint) )
        return NULL;

    int fd = open_file(filename, O_RDONLY, 0, errprint);
    if( fd == -1 )
        return NULL;

    file_buf = mmap(NULL, status->st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if( file_buf == MAP_FAILED ) {
        file_buf = NULL;
        int error = errno;
        if( errprint )
            fprintf(stderr, "%s - mmap() failure: %s\n", 
                    __FUNCTION__, strerror(error));
    }

    return file_buf;
}

inline int unmap_file( void* file_buf, size_t file_len, int errprint=1 )
{
    if( file_buf != NULL ) {
        int rc = munmap( MUNMAP_CAST file_buf, file_len );
        if( rc == -1 ) {
            int error = errno;
            if( errprint )
                fprintf(stderr, "%s - munmap() failure: %s\n", 
                        __FUNCTION__, strerror(error));
        }
        return rc;
    }
    return -1;
}


//-------------------------- Misc Data Processing --------------------------

/* String & Character Checks */

inline bool string_is_numeric(char* s)
{
    char* c = s;
    while( *c != '\0' ) {
        if( ! isdigit(*c) ) return false;
        c++;
    } 
    return true;
}

/* Get offsets of lines contained within a character buffer */
inline void get_lines(const char* data, unsigned data_len, vector<unsigned>& line_offsets)
{
    const char* line = data;
    while( line < (data + data_len) ) {
        unsigned curr_offset = unsigned(line - data);
        if( *line != '\0' )
            line_offsets.push_back(curr_offset);
        const char* found = (const char*) memchr(line, '\n', data_len - curr_offset);
        if(found)
            line = found + 1;
        else
            break;
    }
}

#endif /* _FILE_UTIL_H_ */
