/**************************************************************************
 * Copyright 2003-2010   Michael J. Brim, Barton P. Miller                *
 *                Detailed MRNet usage rights in "LICENSE" file.          *
 **************************************************************************/

#include "mrnet_lightweight/MRNet.h"
#include "HeteroFilters_lightweight.h"
#include <sys/stat.h>
#include <assert.h>

int main(int argc, char **argv)
{
    int tag = 0;
    Stream_t * stream;
    Packet_t* p = (Packet_t*)malloc(sizeof(Packet_t));
    Network_t * net;

    int blocksz = 0, max_blocks = 0, num_blocks = 0;
    char *file = NULL, *contents = NULL;
    struct stat s;
    size_t length = 0;
    int mapped = 0;
    
    assert(p);

    net = Network_CreateNetworkBE( argc, argv );
    assert(net);

    do {
        if( Network_recv(net,&tag, p, &stream) != 1 ) {
            fprintf(stderr, "stream::recv() failure\n");
            break;
        }

        switch(tag) {

        case PROT_INIT:
            fprintf( stdout, "BE: Processing PROT_INIT ...\n");
            Packet_unpack(p, "%s %d", &file, &blocksz );
            
            contents = (char*) map_file(file, &s, 0);
            if( contents != NULL  ) 
                mapped = 1;
            else if( (contents = read_file(file, length)) != NULL )
                s.st_size = length;
            else {
                fprintf(stderr, "ERROR: Unable to read %s\n", file);
                fflush( stderr );
                break;
            }

            num_blocks = s.st_size / blocksz;
            if( s.st_size % blocksz ) num_blocks++;
            Stream_send(stream,tag, "%d", num_blocks);
            Stream_flush(stream);
            
            break;

        case PROT_SEARCH: {
            fprintf( stdout, "BE: Processing PROT_SEARCH ...\n");
            max_blocks = 0;
            Packet_unpack(p, "%d", &max_blocks );
            
            off_t offset = 0;
            int i;
            for( i=0; i < max_blocks; i++ ) {
                if( offset >= s.st_size ) {
                    // empty message to indicate end of data
                    Stream_send(stream,tag, "%ac", NULL, 0);
                }
                else if( offset + blocksz > s.st_size ) {
                    Stream_send(stream,tag, "%ac", contents + offset,
                                 s.st_size - offset);
                    offset = s.st_size;
                }
                else {
                    Stream_send(stream,tag, "%ac", contents + offset, blocksz);
                    offset += blocksz;
                }
                Stream_flush(stream);
            }
            break;
        }

        case PROT_EXIT:
            if( mapped )
                unmap_file((void*)contents, (size_t)s.st_size, 1);
            else
                free(contents);

            if( Stream_send(stream,tag, "%d", 0) == -1 ){
                fprintf(stderr, "stream::send(%%s) failure\n");
                break;
            }
            Stream_flush(stream);
            break;

        default:
            fprintf(stderr, "BE: ERROR: Unknown Protocol: %d\n", tag);
            tag = PROT_EXIT;
            break;
        }

        fflush( stdout );
        fflush( stderr );

    } while ( tag != PROT_EXIT );
       
    if ( p != NULL)
        free(p);

    // wait for final teardown packet from FE
    Network_waitfor_ShutDown(net);
    if (net != NULL)
        delete_Network_t(net);

    return 0;
}
