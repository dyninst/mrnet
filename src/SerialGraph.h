/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#if !defined( SerialGraph_h )
#define SerialGraph_h 1

#include <string>
#include <iomanip>
#include "utils.h"

namespace MRN
{

class SerialGraph {
 private:
    std::string _byte_array;
    size_t _buf_idx;
    unsigned int _num_nodes;
    unsigned int _num_backends;

 public:
    SerialGraph( const char * ibyte_array )
        :_byte_array(ibyte_array), _num_nodes(0), _num_backends(0) { }

    SerialGraph( std::string ibyte_array )
        :_byte_array(ibyte_array), _num_nodes(0), _num_backends(0) { }

    SerialGraph() :_num_nodes(0), _num_backends(0) { }
    ~SerialGraph() { }

    void add_Leaf( std::string, Port, Rank );
    void add_SubTreeRoot( std::string, Port, Rank );
    void end_SubTree(void);
    std::string get_ByteArray(void) { return _byte_array; }
    void print(void) { fprintf( stderr, "%s\n", _byte_array.c_str() ); }

    std::string get_RootHostName(void);
    Port get_RootPort(void);
    Rank get_RootRank(void);
    SerialGraph * get_MySubTree( std::string &ihostname, Port port, Rank irank );
    void set_ToFirstChild(void);

    bool set_Port(std::string hostname, Port port, Rank irank); 

    SerialGraph *get_NextChild(  );
    bool is_RootBackEnd(void) const;
};

} /* namespace MRN */

#endif /* SerialGraph_h */
