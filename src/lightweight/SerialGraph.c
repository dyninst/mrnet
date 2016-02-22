/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include "SerialGraph.h"
#include "mrnet_lightweight/Network.h"

SerialGraph_t* new_SerialGraph_t(char* ibyte_array)
{
    SerialGraph_t* sg;
    
    mrn_dbg_func_begin();
   
    sg = (SerialGraph_t*) calloc( (size_t)1, sizeof(SerialGraph_t) );
    assert(sg != NULL);

    if( ibyte_array != NULL ) {
        sg->byte_array = strdup(ibyte_array);
        sg->arr_len = strlen(ibyte_array) + 1;        
        mrn_dbg(5, mrn_printf(FLF, stderr, 
                              "Creating new serial graph with byte_array='%s'\n", 
                              sg->byte_array));
    } else {
        sg->arr_len = 1024; /* pre-allocation */
        sg->byte_array = (char*) malloc((size_t)sg->arr_len);
        assert(sg->byte_array);
        sg->byte_array[0] = '\0';
        mrn_dbg(5, mrn_printf(FLF, stderr, 
                              "Creating new serial graph with empty byte_array\n"));
    }

    mrn_dbg_func_end();
    return sg;
}

void delete_SerialGraph_t(SerialGraph_t* sg)
{
    if( NULL != sg) {
        if( NULL != sg->byte_array ) 
            free(sg->byte_array);
        free(sg);
    }
}

char* SerialGraph_get_ByteArray(SerialGraph_t* sg)
{
    return sg->byte_array;
}

SerialGraph_t* SerialGraph_get_MySubTree(SerialGraph_t* sg, char* ihostname, 
                                         Port iport, Rank irank) 
{ 
    char hoststr[256];
    size_t cur, end;
    int num_leftbrackets=1, num_rightbrackets=0;
    char *byte_array, *new_byte_array;
    SerialGraph_t* retval;

    mrn_dbg_func_begin();

    sprintf(hoststr, "[%s:%05hu:%u:", ihostname, iport, irank);
    mrn_dbg(5, mrn_printf(FLF, stderr, "looking for SubTreeRoot: '%s'\n", hoststr));
 
    byte_array = sg->byte_array;
    byte_array = strstr(byte_array, hoststr);
    if (byte_array == NULL) {
        mrn_dbg(3, mrn_printf(FLF, stderr,
                              "No SubTreeRoot: '%s' found in byte_array: '%s'\n",
                              hoststr, sg->byte_array));
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

    new_byte_array = (char*) malloc((size_t)end+1);
    if( new_byte_array == NULL ) {
        mrn_dbg(1, mrn_printf(FLF, stderr, "malloc(%" PRIszt ") failed\n", end+1));
        exit(0);
    }    
    strncpy(new_byte_array, byte_array, end);
    new_byte_array[end] = '\0';
 
    retval = new_SerialGraph_t(new_byte_array);

    // free malloc'd stuff
    free(new_byte_array);

    mrn_dbg(5, mrn_printf(FLF, stderr, "returned sg byte array '%s'\n", 
                          retval->byte_array));
    mrn_dbg_func_end();
    return retval;
}

void SerialGraph_add_Leaf(SerialGraph_t* sg, char* ihostname, 
                          Port iport, Rank irank)
{
    char hoststr[256];
    size_t len;
    
    mrn_dbg_func_begin();

    len = (size_t) sprintf(hoststr, "[%s:%05hu:%u:0]", ihostname, iport, irank);
    mrn_dbg(5, mrn_printf(FLF, stderr, "adding sub tree leaf: %s\n", hoststr));

    len += strlen(sg->byte_array) + 1;
    if( len > sg->arr_len ) {
        len *= 2;
        sg->byte_array = (char*)realloc(sg->byte_array, len);
        if( sg->byte_array == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "realloc(%" PRIszt ") failed\n", len));
            exit(0);
        }
        sg->arr_len = len;
    }
    strcat(sg->byte_array, hoststr);

    sg->num_nodes++;
    sg->num_backends++;

    mrn_dbg_func_end();
}

void SerialGraph_add_SubTreeRoot(SerialGraph_t* sg, char* ihostname, 
                                 Port iport, Rank irank)
{
    char hoststr[256];
    size_t len;

    mrn_dbg_func_begin();

    len = (size_t) sprintf(hoststr, "[%s:%05hu:%u:1", ihostname, iport, irank);
    mrn_dbg(5, mrn_printf(FLF, stderr, "adding sub tree root: %s\n", hoststr));

    len += strlen(sg->byte_array) + 1;
    if( len > sg->arr_len ) {
        len *= 2;
        sg->byte_array = (char*) realloc(sg->byte_array, len);
        if( sg->byte_array == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "realloc(%" PRIszt ") failed\n", len));
            exit(0);
        }
        sg->arr_len = len;
    }
    strcat(sg->byte_array, hoststr);

    sg->num_nodes++;

    mrn_dbg_func_end();
}

void SerialGraph_end_SubTree(SerialGraph_t* sg)
{
    size_t curlen, len;

    mrn_dbg_func_begin();

    curlen = strlen(sg->byte_array);
    len = curlen + 2; // appending ']' + '\0'
    if( len > sg->arr_len ) {
        len *= 2;
        sg->byte_array = (char*) realloc(sg->byte_array, len);
        if( sg->byte_array == NULL ) {
            mrn_dbg(1, mrn_printf(FLF, stderr, "realloc(%" PRIszt ") failed\n", len));
            exit(0);
        }
        sg->arr_len = len;
    }

    sg->byte_array[curlen] = ']';
    sg->byte_array[curlen+1] = '\0';

    mrn_dbg_func_end();
}

void SerialGraph_set_ToFirstChild(SerialGraph_t* sg) 
{
    char* buf;
    const char* delim;
    size_t val;
    
    buf = sg->byte_array;
    delim = "[";
    val = strcspn(buf+1, delim);
    sg->buf_idx = val + 1;
    mrn_dbg(5, mrn_printf(FLF, stderr, "set_ToFirstChild: buf_idx=%" PRIszt "\n", 
                          sg->buf_idx));
}

char* SerialGraph_get_RootHostName(SerialGraph_t* sg) 
{
    char *buf, *retval;
    const char* delim;
    size_t len;
    
    // start searching at index 1
    buf = sg->byte_array + 1; 

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

Port SerialGraph_get_RootPort(SerialGraph_t* sg)
{
    char *buf, *port_string;
    const char* delim;
    size_t loc;
    Port retval;
    
    // start searching at index 1
    buf = sg->byte_array + 1;
    
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

Rank SerialGraph_get_RootRank(SerialGraph_t* sg)
{
    char *buf, *rank_string;
    const char* delim;
    size_t loc;
    Rank retval;
    
    // start searching at index 1 
    buf = sg->byte_array + 1;

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

SerialGraph_t* SerialGraph_get_NextChild(SerialGraph_t* sg)
{
    size_t end, cur;
    int num_leftbrackets=1, num_rightbrackets=0;
    char *buf, *buf2, *buf3;
    SerialGraph_t* retval;

    mrn_dbg_func_begin();

    cur = sg->buf_idx;
    if( cur > sg->arr_len )
        return NULL;

    buf = sg->byte_array;
    buf2 = buf + cur;
    if( NULL == strchr(buf2, '[') ) {
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
    sg->buf_idx = cur + 1;

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

int SerialGraph_is_RootBackEnd(SerialGraph_t* sg) 
{
    //format is "host:port:rank:[0|1]: where 0 means back-end and 1 means internal node
    size_t idx;
    unsigned int num_colons;
    char typ;
    for( idx=0, num_colons=0; num_colons < 3; idx++ ) {
        if( ':' == sg->byte_array[idx] ) {
            num_colons++;
        }
    }

    typ = sg->byte_array[idx];
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

    // Should never get here
    return -1;
}

int SerialGraph_set_Port(SerialGraph_t* sg, char * hostname, Port port, Rank irank)
{
    size_t begin_len;
    char *old_byte_array, *new_byte_array, *begin_str;
    char old_hoststr[256];
    char new_hoststr[256];

    sprintf(old_hoststr, "[%s:%05hu:%u:", hostname, UnknownPort, irank);
    sprintf(new_hoststr, "[%s:%05hu:%u:", hostname, port, irank);
    
    old_byte_array = sg->byte_array;
    new_byte_array = (char*) malloc( strlen(old_byte_array) + 10 );
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
    new_byte_array[begin_len] = '\0';

    // add the updated hoststr
    strcat(new_byte_array, new_hoststr);
    
    // concat on the remainder of the old byte array
    begin_str += strlen(old_hoststr);
    strcat(new_byte_array, begin_str);

    // swap the byte arrays
    free(old_byte_array);
    sg->byte_array = new_byte_array;

    return true;
}
