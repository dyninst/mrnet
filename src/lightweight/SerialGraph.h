/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined(__serial_graph_h)
#define __serial_graph_h 1

#include "utils_lightweight.h"

struct SerialGraph_t {
    char* byte_array;
    size_t arr_len;
    size_t buf_idx;
    unsigned int num_nodes;
    unsigned int num_backends;
};

typedef struct SerialGraph_t SerialGraph_t;

char* SerialGraph_get_ByteArray(SerialGraph_t * sg);

SerialGraph_t* new_SerialGraph_t(char* byte_array);

void delete_SerialGraph_t(SerialGraph_t* sg);

SerialGraph_t* SerialGraph_get_MySubTree(SerialGraph_t* serial_graph, 
                                         char* ihostname, Port iport, Rank irank);

void SerialGraph_add_Leaf(SerialGraph_t* serial_graph, char* hostname, 
                          Port iport, Rank irank);

void SerialGraph_add_SubTreeRoot(SerialGraph_t* serial_graph, char* hostname, 
                                 Port iport, Rank irank);

void SerialGraph_end_SubTree(SerialGraph_t* serial_graph);

void SerialGraph_set_ToFirstChild(SerialGraph_t* serial_graph);

char* SerialGraph_get_RootHostName(SerialGraph_t* serial_graph);

Port SerialGraph_get_RootPort(SerialGraph_t* serial_graph);

Rank SerialGraph_get_RootRank(SerialGraph_t* serial_graph);

SerialGraph_t* SerialGraph_get_NextChild(SerialGraph_t* serial_graph);

int SerialGraph_is_RootBackEnd(SerialGraph_t* serial_graph);

int SerialGraph_set_Port(SerialGraph_t * serial_graph, char * hostname, 
                         Port port, Rank irank);


#endif /* __serial_graph_h */
