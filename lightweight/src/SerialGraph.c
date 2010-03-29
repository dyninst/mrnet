/****************************************************************************
 *  Copyright 2003-2009 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include "mrnet/Network.h"
#include "SerialGraph.h"
#include "mrnet/Types.h"
#include "utils_lightweight.h"

SerialGraph_t* new_SerialGraph_t(char* ibyte_array)
{
    SerialGraph_t* serial_graph;
    
    mrn_dbg_func_begin();
    
    serial_graph = (SerialGraph_t*)malloc(sizeof(SerialGraph_t));
    assert(serial_graph != NULL);

    if (ibyte_array) {
        serial_graph->byte_array = (char*)malloc(sizeof(char)*strlen(ibyte_array)+1);
        strncpy(serial_graph->byte_array, ibyte_array, strlen(ibyte_array)+1);
        mrn_dbg(5, mrn_printf(FLF, stderr, "Creating new serial graph with byte_array=%s\n", serial_graph->byte_array));
    } else {
        serial_graph->byte_array = (char*)malloc(sizeof(char)*2);
        assert(serial_graph->byte_array);
        strcpy(serial_graph->byte_array, "");
        mrn_dbg(5, mrn_printf(FLF, stderr, "Creating new serial graph with byte_array=NULL\n"));
    }

    mrn_dbg_func_end();
    return serial_graph;
}

SerialGraph_t* SerialGraph_get_MySubTree(SerialGraph_t* _serial_graph, char*  ihostname, Port iport, Rank irank) 
{ 
    char* hoststr;
    int size;
    char* byte_array = _serial_graph->byte_array;
    size_t cur, end;
    char* port;
    char* rank;
    int num_leftbrackets=1, num_rightbrackets=0;
    char* new_byte_array;
    SerialGraph_t* retval;

    mrn_dbg_func_begin();

    port = (char*)malloc(sizeof(char)*20);
    assert(port != NULL);
    sprintf(port, "%d", iport);
    rank = (char*)malloc(sizeof(char)*20);
    assert(port != NULL);
    sprintf(rank, "%d", irank);

    size = 5;
    size += strlen(ihostname);
    size += strlen(port);
    size += strlen(rank);

    hoststr = (char*)malloc(sizeof(char)*size);

    strcpy(hoststr, ""); 
    strcat(hoststr, "["); //[
    strcat(hoststr, ihostname); //[ihostname
    strcat(hoststr, ":"); // [ihostname:
    strcat(hoststr, port); //[ihostname:iport
    strcat(hoststr, ":"); //[ihostname:iport:
    strcat(hoststr, rank); //[ihostname:iport:irank
    strcat(hoststr, ":"); //[ihostname:iport:irank:

    mrn_dbg(5, mrn_printf(FLF, stderr, "SubTreeRoot:\"%s\" byte_array=\"%s\"\n", hoststr, byte_array)); 
    byte_array = strstr(byte_array, hoststr);

    if (byte_array == NULL) {
        mrn_dbg(1, mrn_printf(FLF, stderr,
              "No SubTreeRoot:\"%s\" found in byte_array:\"%s\"\n",
                hoststr, _serial_graph->byte_array));
    free(hoststr);
    free(port);
    free(rank);
    mrn_dbg_func_end();
    return NULL;
  }

    //now find matching ']'
    cur = 0;
    end = 1;
    while (num_leftbrackets != num_rightbrackets) {
      cur++,end++;
      if(byte_array[cur] == '[')
        num_leftbrackets++;
      else if (byte_array[cur] == ']')
        num_rightbrackets++;
    }

    new_byte_array = (char*)malloc(sizeof(char)*(strlen(byte_array)+1));
    assert(new_byte_array);
    strncpy(new_byte_array, byte_array, end);
    retval = new_SerialGraph_t(new_byte_array);

    // free malloc'd stuff
    free(hoststr);
    free(port);
    free(rank);

    mrn_dbg(5, mrn_printf(FLF, stderr, "returned sg byte array :\"%s\"\n", retval->byte_array));

    mrn_dbg_func_end();
    return retval;
}

void SerialGraph_add_Leaf(SerialGraph_t* serial_graph, char* ihostname, Port iport, Rank irank)
{
    char* hoststr;
    char* port;
    char* rank;
    
    mrn_dbg_func_begin();
    hoststr = (char*)malloc(sizeof(char)*256);
    assert(hoststr);

    port = (char*)malloc(sizeof(char)*20);
    assert(port);
    sprintf(port, "%d", iport);
    rank = (char*)malloc(sizeof(char)*20);
    assert(rank);
    sprintf(rank, "%d", irank);

    strcpy(hoststr, ""); 
    strcat(hoststr, "["); //[
    strcat(hoststr, ihostname); //[ihostname
    strcat(hoststr, ":"); // [ihostname:
    strcat(hoststr, port); //[ihostname:iport
    strcat(hoststr, ":"); //[ihostname:iport:
    strcat(hoststr, rank); //[ihostname:iport:irank
    strcat(hoststr, ":0]"); //[ihostname:iport:irank:0]

    mrn_dbg(5, mrn_printf(FLF, stderr, "byte_array: %s\n", serial_graph->byte_array));
    mrn_dbg(5, mrn_printf(FLF, stderr, "adding leaf: %s\n", hoststr));
    
    serial_graph->byte_array = (char*)realloc(serial_graph->byte_array, sizeof(char)*(strlen(serial_graph->byte_array) + strlen(hoststr) + 1));
    strcat(serial_graph->byte_array, hoststr);
    
    mrn_dbg(5, mrn_printf(FLF, stderr, "byte_array: %s\n", serial_graph->byte_array));

    serial_graph->num_nodes++;
    serial_graph->num_backends++;

    free(hoststr);
    free(port);
    free(rank);
    mrn_dbg_func_end();
}

void SerialGraph_add_SubTreeRoot(SerialGraph_t* serial_graph, char* ihostname, Port iport, Rank irank)
{
  char* hoststr;
  char* port;
  char* rank;

  mrn_dbg_func_begin();
  hoststr = (char*)malloc(sizeof(char)*256);
  assert(hoststr);

  port = (char*)malloc(sizeof(char)*20);
  assert(port);
  sprintf(port, "%d", iport);
  rank = (char*)malloc(sizeof(char)*20);
  assert(rank);
  sprintf(rank, "%d", irank);
 
  strcpy(hoststr, ""); 
  strcat(hoststr, "["); //[
  strcat(hoststr, ihostname); //[ihostname
  strcat(hoststr, ":"); // [ihostname:
  strcat(hoststr, port); //[ihostname:iport
  strcat(hoststr, ":"); //[ihostname:iport:
  strcat(hoststr, rank); //[ihostname:iport:irank
  strcat(hoststr, ":1"); //[ihostname:iport:irank:0]

  mrn_dbg(5, mrn_printf(FLF, stderr, "adding sub tree root:%s\n", hoststr));

  serial_graph->byte_array = (char*)realloc(serial_graph->byte_array, sizeof(char)*(strlen(serial_graph->byte_array) + strlen(hoststr) + 1));
  strcat(serial_graph->byte_array, hoststr);
  serial_graph->num_nodes++;

  free(hoststr);
  free(port);
  free(rank);
  mrn_dbg_func_end();

}

void SerialGraph_end_SubTree(SerialGraph_t* serial_graph)
{
  mrn_dbg_func_begin();
  serial_graph->byte_array = (char*)realloc(serial_graph->byte_array, sizeof(char)*(strlen(serial_graph->byte_array)+2));
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
    serial_graph->buf_idx = val + 1; // offset was 1
    mrn_dbg(5, mrn_printf(FLF, stderr, "set_ToFirstChild: byte_array=%s, buf_idx=%d\n", serial_graph->byte_array, serial_graph->buf_idx));
}

char* SerialGraph_get_RootHostName(SerialGraph_t* serial_graph) 
{
    char* buf;
    char* delim;
    int len;
    char* retval;
    
    buf = serial_graph->byte_array;
    buf++; // want to start searching at index 1

    delim = ":";
    len = strcspn(buf, delim);
    retval = (char*)malloc(sizeof(char)*len+1);
    assert(retval);
    strncpy(retval, buf, len);
    retval[len]='\0';
    
    return retval;    
}

Port SerialGraph_get_RootPort(SerialGraph_t* serial_graph)
{
    char* buf;
    char* delim;
    int loc;
    int i;
    char* port_string;
    Port retval;
    
    buf = (char*)malloc(sizeof(char)*(strlen(serial_graph->byte_array)+1));
    assert(buf);
    strncpy(buf, serial_graph->byte_array, strlen(serial_graph->byte_array)+1);
    buf++;
    buf++; // want to start looking at index 2
 

    // find location of ":"
    delim = ":";
    loc = strcspn(buf, delim);
    for (i = 0; i <= loc; i++)
        buf++;
    
    
    // find location of next ":"
    loc = strcspn(buf, delim);
    port_string = (char*)malloc(sizeof(char)*loc);
    assert(port_string);
    strncpy(port_string, buf, loc);

    retval = atoi(port_string);

    free(port_string);

    return retval;
}

Rank SerialGraph_get_RootRank(SerialGraph_t* serial_graph)
{
    Rank retval = UnknownRank;
    size_t begin=0, end=1;
    char* buf;
    char* delim;
    char* tok;

    mrn_dbg_func_begin();

    buf = (char*)malloc(sizeof(char)*(strlen(serial_graph->byte_array)+1));
    assert(buf);
    strncpy(buf, serial_graph->byte_array, strlen(serial_graph->byte_array)+1);

    delim = ":";
    tok = (char*)malloc(sizeof(char)*256);
    assert(tok);
    tok = strtok(buf, delim);
    if (tok) {
        tok = strtok(NULL,delim);
        delim = " ";
        if (tok) {
            tok = strtok(NULL, delim);

            retval = atoi(tok);
        }
    }
    
    mrn_dbg_func_end();

    return retval;
}

SerialGraph_t* SerialGraph_get_NextChild(SerialGraph_t* serial_graph)
{
    
    size_t begin, end, cur;
    const char* buf = serial_graph->byte_array;
    int i;
    char* buf2;
    char delim;
    int num_leftbrackets=1, num_rightbrackets=0;
    char* buf3;
    SerialGraph_t* retval;

    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, "byte_array = %s\n", buf));

    buf2 = serial_graph->byte_array;
    if (serial_graph->buf_idx != -1) {
        for (i = 0; i < serial_graph->buf_idx; i++)
            buf2++;
    }
    
    mrn_dbg(5, mrn_printf(FLF, stderr, "buf2 = %s\n", buf2));

    delim = '[';
    if (serial_graph->buf_idx == -1 ||
        strchr(buf2, delim) == NULL) {
        return NULL;
    }

    cur = serial_graph->buf_idx;
    begin = serial_graph->buf_idx;
    end = 1;
    while (num_leftbrackets != num_rightbrackets) {
        cur++, end++;
        if (buf[cur] == '[')
            num_leftbrackets++;
        else if (buf[cur] == ']')
            num_rightbrackets++;
    }
    serial_graph->buf_idx = cur+1;

    mrn_dbg(5, mrn_printf(FLF, stderr, "begin=%d, end=%d\n", begin, end));

    // get substr from begin to end
    for (i = 0; i < begin; i++)
       buf++;
    buf3 = (char*)malloc(sizeof(char)*end+1);
    assert(buf3 != NULL);
    strncpy(buf3, buf, end);
    buf3[end] = '\0';

    retval = new_SerialGraph_t(buf3);
    mrn_dbg(5, mrn_printf(FLF, stderr, "returned sg byte array :\"%s\"\n", retval->byte_array));

    mrn_dbg_func_end();

    return retval;
}

int SerialGraph_is_RootBackEnd(SerialGraph_t* serial_graph) 
{
    //format is "host:port:rank:[0|1]: where 0 means back-end and 1 means internal node
    unsigned int idx, num_colons;
    for (idx = 0, num_colons=0; num_colons<3; idx++) {
        if (serial_graph->byte_array[idx] == ':') {
            num_colons++;
        }
    }
    if (serial_graph->byte_array[idx] == '0') {
        return 1;
    }
    else {
        return 0;
    }
}
