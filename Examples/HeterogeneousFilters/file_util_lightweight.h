/**************************************************************************
 * Copyright 2003-2015   Michael J. Brim, Barton P. Miller                *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#ifndef _FILE_UTIL_LTWT_H_
#define _FILE_UTIL_LTWT_H_

/* File Handling Utility Functions */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef __FUNCTION__
#define __FUNCTION__ "nofunction"
#endif

#ifdef os_solaris
#define MUNMAP_CAST (char*)
#else
#define MUNMAP_CAST
#endif

int stat_file(const char* filename, struct stat* status, int errprint)
{
   if( stat(filename, status) ) {
      int err = errno;
      if( errprint ) {
         fprintf(stderr, "%s - stat(%s) failure: %s\n", 
                 __FUNCTION__, filename, strerror(err));
      }
      return -err;
   }
   return 0;
}

int open_file(const char* filename, int flags, mode_t mode, int errprint)
{
   int fd = open(filename, flags, mode);
   if( fd == -1 ) {
      int err = errno;
      if( errprint ) {
         fprintf(stderr, "%s - open(%s, %d) failure: %s\n", 
                 __FUNCTION__, filename, flags, strerror(err));
      }
      return -err;
   }
   return fd;
}

char* read_file(const char* filename, size_t filelen)
{
   /* ifstream f; */
    FILE* f;
    size_t len;
    char* contents = NULL;
    filelen = 0;
    f = fopen(filename, "r");
    if (f) {
        // get file length
        len = 0;
        int valid = fgetc(f);
        while (valid != EOF) {
            len++;
            valid = fgetc(f);
            if( len == 65536 ) // is this conservative?
                break;
        }
        f = freopen(filename, "r", f);

        // read entire file
        contents = (char*)malloc(len+512);
        if (contents == NULL)
            fprintf(stderr, "%s[%d] %s - malloc(%zd) failed\n",
                    __FILE__,__LINE__,__FUNCTION__, len+1);
        else {
            fread(contents, sizeof(char), len+511, f);
            contents[len] = '\0';
            filelen = len+1;
        }
        fclose(f);
    }
    else
        fprintf(stderr, "%s[%d] %s - fopen(%s) failed\n",
              __FILE__, __LINE__, __FUNCTION__, filename);

    return contents;
}

void* map_file( const char* filename, struct stat* status, int errprint)
{
   void* file_buf = NULL;

   if( stat_file(filename, status, errprint) )
      return NULL;

   int fd = open_file(filename, O_RDONLY, 0, errprint);
   if( fd == -1 )
      return NULL;

   file_buf = mmap(NULL, (size_t)status->st_size, PROT_READ, MAP_PRIVATE, fd, (size_t)0);
   if( file_buf == MAP_FAILED ) {
      file_buf = NULL;
      int err = errno;
      if( errprint )
         fprintf(stderr, "%s - mmap() failure: %s\n", 
                 __FUNCTION__, strerror(err));
   }

   return file_buf;
}

int unmap_file( void* file_buf, size_t file_len, int errprint)
{
   if( file_buf != NULL ) {
      int rc = munmap( MUNMAP_CAST file_buf, file_len );
      if( rc == -1 ) {
         int err = errno;
         if( errprint )
            fprintf(stderr, "%s - munmap() failure: %s\n", 
                    __FUNCTION__, strerror(err));
      }
      return rc;
   }
   return -1;
}


#endif /* _FILE_UTIL_LTWT_H_ */
