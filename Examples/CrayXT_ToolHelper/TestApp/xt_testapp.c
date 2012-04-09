/****************************************************************************
 * Copyright 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include "mpi.h"

int
main( int argc, char* argv[] )
{
    int ret = 0;
    int wSize = -1;
    int wRank = -1;
    const int nIterations = 10;
    int iter = -1;
    double data;
    int tag = 7;
    int leftNeighborRank;
    int rightNeighborRank;
    MPI_Status status;


    MPI_Init( &argc, &argv );

    /* determine my place in the world */
    MPI_Comm_size( MPI_COMM_WORLD, &wSize );
    MPI_Comm_rank( MPI_COMM_WORLD, &wRank );

    fprintf( stderr, "%d of %d online\n", wRank, wSize );
    fflush( stderr );

    /* determine our left and right neighbor ranks */
    leftNeighborRank = (wRank == 0 ? (wSize - 1) : wRank - 1 );
    rightNeighborRank = (wRank + 1) % wSize;

    /* do something as if we were a real application - we do a ring */
    for( iter = 0; iter < nIterations; iter++ )
    {
        if( wRank == 0 )
        {
            /* 
             * I am rank 0: I have to kick off the ring 
             */

            /* send message to my right neighbor */
            MPI_Send( &data,
                        1,
                        MPI_DOUBLE,
                        rightNeighborRank,
                        tag,
                        MPI_COMM_WORLD );

            /* receive the message after going around the ring */
            MPI_Recv( &data,
                        1,
                        MPI_DOUBLE,
                        leftNeighborRank,
                        tag,
                        MPI_COMM_WORLD,
                        &status );
        }
        else
        {
            /* 
             * I am not rank 0: I just participate in the ring 
             */
            /* receive a message from our left neighbor */
            MPI_Recv( &data,
                        1,
                        MPI_DOUBLE,
                        leftNeighborRank,
                        tag,
                        MPI_COMM_WORLD,
                        &status );

            /* send message along to our right neighbor */
            MPI_Send( &data,
                        1,
                        MPI_DOUBLE,
                        rightNeighborRank,
                        tag,
                        MPI_COMM_WORLD );
        }
        sleep(1);
    }

    fprintf( stderr, "%d of %d offline\n", wRank, wSize );
    fflush( stderr );
    MPI_Finalize();

    return ret;
}

