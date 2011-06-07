/****************************************************************************
 *  Copyright 2003-2011 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include "mrnet_lightweight/Network.h"
#include "SerialGraph.h"
#include "mrnet_lightweight/Types.h"
#include "utils_lightweight.h"

SerialGraph_t* new_SerialGraph_t(char* ibyte_array)
{
    SerialGraph_t* serial_graph;
    
    mrn_dbg_func_begin();
   
    serial_graph = (SerialGraph_t*) malloc(sizeof(SerialGraph_t));
    assert(serial_graph != NULL);

    if( ibyte_array ) {
        serial_graph->byte_array = strdup(ibyte_array);
        serial_graph->arr_len = strlen(ibyte_array) + 1;        
        mrn_dbg(5, mrn_printf(FLF, stderr, 
                              "Creating new serial graph with byte_array='%s'\n", 
                              serial_graph->byte_array));
    } else {
        serial_graph->arr_len = 1024; /* pre-allocation */
        serial_graph->byte_array = (char*)malloc((size_t)serial_graph->arr_len);
        assert(serial_graph->byte_array);
        serial_graph->byte_array[0] = '\0';
        mrn_dbg(5, mrn_printf(FLF, stderr, 
                              "Creating new serial graph with empty byte_array\n"));
    }

    mrn_dbg_func_end();
    return serial_graph;
}

void free_SerialGraph_t(SerialGraph_t* sg)
{
    free(sg->byte_array);
    free(sg);
}

char* SerialGraph_get_ByteArray(SerialGraph_t* sg)
{
    return sg->byte_array;
}

SerialGraph_t* SerialGraph_get_MySubTree(SerialGraph_t* _serial_graph, char* ihostname, 
                                         Port iport, Rank irank) 
{ 
    size_t cur, end;
    int num_leftbrackets=1, num_rightbrackets=0;
    char *hoststr, *byte_array, *new_byte_array;
    SerialGraph_t* retval;

    mrn_dbg_func_begin();

    hoststr = (char*)malloc((size_t)256);
    assert(hoststr);
    sprintf(hoststr, "[%s:%hu:%u:", ihostname, iport, irank);

    mrn_dbg(5, mrn_printf(FLF, stderr, "looking for SubTreeRoot:'%s'\n", hoststr));
 
    byte_array = _serial_graph->byte_array;
    byte_array = strstr(byte_array, hoststr);
    if (byte_array == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "No SubTreeRoot:\"%s\" found in byte_array:\"%s\"\n",
                              hoststr, _serial_graph->byte_array));
        free(hoststr);

        mrn_dbg_func_end();
        return NULL;
    }

    //now find matching ']'
    cur = 0;
    end = 1;
    while( num_leftbrackets != num_rightbrackets ) {
        cur++;
        end++;
        if(byte_array[cur] == '[')
            num_leftbrackets++;
        else if (byte_array[cur] == ']')
            num_rightbrackets++;
    }

    new_byte_array = (char*)malloc((size_t)end+1);
    if( new_byte_array == NULL ) {
        mrn_printf(FLF, stderr, "malloc(%zu) failed\n", end+1);
        exit(0);
    }    
    strncpy(new_byte_array, byte_array, end);
    new_byte_array[end]='\0';
 
    retval = new_SerialGraph_t(new_byte_array);

    // free malloc'd stuff
    free(hoststr);
    free(new_byte_array);

    mrn_dbg(5, mrn_printf(FLF, stderr, "returned sg byte array '%s'\n", 
                          retval->byte_array));

    mrn_dbg_func_end();
    return retval;
}

void SerialGraph_add_Leaf(SerialGraph_t* serial_graph, char* ihostname, 
                          Port iport, Rank irank)
{
    char* hoststr;
    unsigned len;
    
    mrn_dbg_func_begin();

    hoststr = (char*)malloc((size_t)256);
    assert(hoststr);
    len = sprintf(hoststr, "[%s:%hu:%u:0]", ihostname, iport, irank);

    mrn_dbg(5, mrn_printf(FLF, stderr, "adding sub tree leaf:%s\n", hoststr));

    len += strlen(serial_graph->byte_array) + 1;
    if( len > serial_graph->arr_len ) {
        len <<= 1;
        serial_graph->byte_array = (char*)realloc(serial_graph->byte_array, (size_t)len);
        if( serial_graph->byte_array == NULL ) {
            mrn_printf(FLF, stderr, "realloc(%zu) failed\n", len);
            exit(0);
        }
        serial_graph->arr_len = len;
    }
    strcat(serial_graph->byte_array, hoststr);
    free(hoststr);

    serial_graph->num_nodes++;
    serial_graph->num_backends++;

    mrn_dbg_func_end();
}

void SerialGraph_add_SubTreeRoot(SerialGraph_t* serial_graph, char* ihostname, 
                                 Port iport, Rank irank)
{
    char* hoststr;
    unsigned len;

    mrn_dbg_func_begin();

    hoststr = (char*)malloc((size_t)256);
    assert(hoststr);
    len = sprintf(hoststr, "[%s:%hu:%u:1", ihostname, iport, irank);
    
    mrn_dbg(5, mrn_printf(FLF, stderr, "adding sub tree root:%s\n", hoststr));

    len += strlen(serial_graph->byte_array) + 1;
    if( len > serial_graph->arr_len ) {
        len <<= 1;
        serial_graph->byte_array = (char*)realloc(serial_graph->byte_array, (size_t)len);
        if( serial_graph->byte_array == NULL ) {
            mrn_printf(FLF, stderr, "realloc(%zu) failed\n", len);
            exit(0);
        }
        serial_graph->arr_len = len;
    }
    strcat(serial_graph->byte_array, hoststr);
    free(hoststr);

    serial_graph->num_nodes++;

    mrn_dbg_func_end();
}

void SerialGraph_end_SubTree(SerialGraph_t* serial_graph)
{
    unsigned len;

    mrn_dbg_func_begin();

    len += strlen(serial_graph->byte_array) + 1;
    if( len > serial_graph->arr_len ) {
        len <<= 1;
        serial_graph->byte_array = (char*)realloc(serial_graph->byte_array, (size_t)len);
        if( serial_graph->byte_array == NULL ) {
            mrn_printf(FLF, stderr, "realloc(%zu) failed\n", len);
            exit(0);
        }
        serial_graph->arr_len = len;
    }

    strcat(serial_graph->byte_array, "]");

    mrn_dbg_func_end();
}

void SerialGraph_set_ToFirstChild(SerialGraph_t* serial_graph) 
{
    char* buf;
    const char* delim;
    unsigned int val;
    
    buf = serial_graph->byte_array;
    buf++;
    delim = "[";
    val = (unsigned int)strcspn(buf, delim);
    serial_graph->buf_idx = val + 1;
    mrn_dbg(5, mrn_printf(FLF, stderr, "set_ToFirstChild: buf_idx=%d\n", 
                          serial_graph->buf_idx));
}

char* SerialGraph_get_RootHostName(SerialGraph_t* serial_graph) 
{
    char *buf, *retval;
    const char* delim;
    size_t len;
    
    // start searching at index 1
    buf = serial_graph->byte_array + 1; 

    // find location of ":"
    delim = ":";
    len = strcspn(buf, delim);
    
    // get host
    retval = (char*) malloc(len+1);
    assert(retval);
    strncpy(retval, buf, len);
    retval[len] = '\0';
    
    return retval;    
}

Port SerialGraph_get_RootPort(SerialGraph_t* serial_graph)
{
    char *buf, *port_string;
    const char* delim;
    size_t loc, len;
    Port retval;
    
    // start searching at index 1
    buf = serial_graph->byte_array + 1;
    
    // find location of ":"
    delim = ":";
    loc = strcspn(buf, delim);
    buf += loc + 1;

    // find location of next ":"
    loc = strcspn(buf, delim);

    // get port
    port_string = (char*) malloc(loc+1);
    assert(port_string);
    strncpy(port_string, buf, loc);
    port_string[loc] = '\0';
    retval = (Port) atoi(port_string);
    free(port_string);
        
    return retval;
}

Rank SerialGraph_get_RootRank(SerialGraph_t* serial_graph)
{
    char *buf, *rank_string;
    const char* delim;
    size_t loc, len;
    Rank retval;
    
    // start searching at index 1 
    buf = serial_graph->byte_array + 1;

    // find location of 2nd ":"
    delim = ":";
    loc = strcspn(buf, delim);
    buf += loc + 1;
    loc = strcspn(buf, delim);
    buf += loc + 1;

    // find location of next ":"
    loc = strcspn(buf, delim);

    // get rank
    rank_string = (char*) malloc(loc+1);
    assert(rank_string);
    strncpy(rank_string, buf, loc);
    rank_string[loc] = '\0';
    retval = (Port) atoi(rank_string);
    free(rank_string);
        
    return retval;
}

SerialGraph_t* SerialGraph_get_NextChild(SerialGraph_t* serial_graph)
{
    size_t end, cur;
    int num_leftbrackets=1, num_rightbrackets=0;
    char *buf, *buf2, *buf3;
    SerialGraph_t* retval;

    cur = serial_graph->buf_idx;
    buf = serial_graph->byte_array;
    buf2 = buf + cur;

    mrn_dbg_func_begin();

    if( serial_graph->buf_idx == (unsigned int)-1 )
        return NULL;
    else if( NULL == strchr(buf2, '[') ) {
        return NULL;
    }

    end = 1;
    while( num_leftbrackets != num_rightbrackets ) {
        cur++;
        end++;
        if( buf[cur] == '[' )
            num_leftbrackets++;
        else if( buf[cur] == ']' )
            num_rightbrackets++;
    }
    serial_graph->buf_idx = cur + 1;

    // get substr from start pos to end
    buf3 = (char*) malloc(end+1);
    assert(buf3 != NULL);
    strncpy(buf3, buf2, end);
    buf3[end] = '\0';

    retval = new_SerialGraph_t(buf3);
    free(buf3);
    mrn_dbg(5, mrn_printf(FLF, stderr, "returned sg byte array '%s'\n", 
                          retval->byte_array));

    mrn_dbg_func_end();

    return retval;
}

int SerialGraph_is_RootBackEnd(SerialGraph_t* serial_graph) 
{
    //format is "host:port:rank:[0|1]: where 0 means back-end and 1 means internal node
    char typ;
    unsigned int idx, num_colons;
    for( idx=0, num_colons=0; num_colons < 3; idx++ ) {
        if (serial_graph->byte_array[idx] == ':') {
            num_colons++;
        }
    }

    typ = serial_graph->byte_array[idx];
    if( typ == '0' ) {
        return 1;
    }
    else if ( typ == '1' ) {
        return 0;
    }
    else {
        mrn_dbg(1, mrn_printf(FLF, stderr, 
                              "unexpected character '%c' in node type field\n", typ));
        assert(false);
    }
}

int SerialGraph_set_Port(SerialGraph_t* serial_graph, char * hostname, Port port, Rank irank)
{
    size_t begin_len;
    char *old_byte_array, *new_byte_array, *begin_str;

    char* old_hoststr = (char*) malloc((size_t)256);
    char* new_hoststr = (char*) malloc((size_t)256);
    assert(old_hoststr);
    assert(new_hoststr);
    sprintf(old_hoststr, "[%s:%hu:%u:", hostname, UnknownPort, irank);
    sprintf(new_hoststr, "[%s:%hu:%u:", hostname, port, irank);
    
    old_byte_array = serial_graph->byte_array;
    new_byte_array = (char*)malloc( strlen(old_byte_array) + 10 );
    assert(new_byte_array);

    // find the old hoststr in the old byte array
    begin_str = strstr(old_byte_array, old_hoststr);
    if( ! begin_str ) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
                              "'%s' not found in the byte_array\n", old_hoststr));
        return false;
    }
    begin_len = begin_str - old_byte_array;
    
    // copy the beginning portion
    strncpy(new_byte_array, old_byte_array, begin_len);

    // add the updated hoststr
    strcat(new_byte_array, new_hoststr);
    
    // concat on the remainder of the old byte array
    begin_str += strlen(old_hoststr);
    strcat(new_byte_array, begin_str);

    // swap the byte arrays
    free(old_byte_array);
    serial_graph->byte_array = new_byte_array;

    // free things that were malloc'd
    free(old_hoststr);
    free(new_hoststr);

    return true;
}
