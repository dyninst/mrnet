/**************************************************************************
 * Copyright 2003-2009   Michael J. Brim, Barton P. Miller                *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include "mrnet/MRNet.h"
#include "HeteroFilters.h"

using namespace MRN;

int main(int argc, char **argv)
{
    int tag = 0;
    Stream * stream = NULL;
    PacketPtr p;

    int blocksz = 0, max_blocks = 0, num_blocks = 0;
    char *file = NULL, *contents = NULL;
    struct stat s;
    size_t length = 0;
    bool mapped = false;

    Network * network = new Network( argc, argv );

    do{
        if( network->recv(&tag, p, &stream) != 1 ) {
            fprintf(stderr, "stream::recv() failure\n");
            return -1;
        }

        switch(tag){

        case PROT_INIT: {
    
            fprintf( stdout, "BE: Processing PROT_INIT ...\n");

            p->unpack( "%s %d", &file, &blocksz );
            contents = (char*) map_file(file, &s, 0);
            if( contents != NULL  ) 
                mapped = true;
            else if( (contents = read_file(file, length)) != NULL )
                s.st_size = length;
            else {
                fprintf(stderr, "ERROR: Unable to read %s\n", file);
                fflush( stderr );
                break;
            }

            num_blocks = s.st_size / blocksz;
            if( s.st_size % blocksz ) num_blocks++;
            stream->send(tag, "%d", num_blocks);
            stream->flush();
            
            break;
        }
        case PROT_SEARCH: {
            fprintf( stdout, "BE: Processing PROT_SEARCH ...\n");

            max_blocks = 0;
            p->unpack( "%d", &max_blocks );
            
            off_t offset = 0;
            for( int i=0; i < max_blocks; i++ ) {
                if( offset >= s.st_size ) {
                    // empty message to indicate end of data
                    stream->send(tag, "%ac", NULL, 0);
                    //fprintf( stdout, "BE: - sent empty block\n");
                }
                else if( offset + blocksz > s.st_size ) {
                    stream->send(tag, "%ac", contents + offset,
                                 s.st_size - offset);
                    //fprintf( stdout, "BE: - sent partial block (%d bytes)\n", s.st_size - offset);
                    offset = s.st_size;
                }
                else {
                    stream->send(tag, "%ac", contents + offset, blocksz);
                    //fprintf( stdout, "BE: - sent whole block (%d bytes)\n", blocksz);
                    offset += blocksz;
                }
                stream->flush();
                //fflush(stdout);
            }
            break;
        }
        case PROT_EXIT: {

            fprintf( stdout, "BE: Processing PROT_EXIT ...\n");

            if( mapped )
                unmap_file((void*)contents, s.st_size);
            else
                free(contents);

            if( stream->send(tag, "%d", 0) == -1 ){
                fprintf(stderr, "stream::send(%%s) failure\n");
                return -1;
            }
            stream->flush();
            fflush( stdout );
            break;
        }
        default:
            fprintf(stderr, "BE: ERROR: Unknown Protocol: %d\n", tag);
            fflush( stderr );
            break;
        }
    } while ( tag != PROT_EXIT );
    sleep(10); // wait for network termination

    return 0;
}
