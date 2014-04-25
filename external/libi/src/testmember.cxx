/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/time.h>
#include "libi/libi_api.h"

using namespace std;
void checkEnv(char* name, std::ofstream& fout);
//timeval t6;
//double duration( timeval t5, timeval t6 ){
//    return ((double)t6.tv_sec + ((double)t6.tv_usec * 0.000001)) - ((double)t5.tv_sec + ((double)t5.tv_usec * 0.000001));
//}
double get_dtime( double offset ){
    timeval t6;
    gettimeofday(&t6, NULL);
    //return (double)t6.tv_sec + ((double)t6.tv_usec * 0.000001) + offset;
    return ((double)t6.tv_sec + (double)t6.tv_usec / 1000000.0) + offset;
}
void get_range( double* res, unsigned int sz, double & min, double & max, int & rin, int & rax ){
    min = max = res[0];
    rin = rax = 0;
    for(int i=1; i< sz; i++){
        if( min > res[i] ){
            min = res[i];
            rin = i;
        }
        if( max < res[i] ){
            max = res[i];
            rax = i;
        }
    }
}
void get_offset(double & offset, char* root){
    char* cmd;
//    cmd="/usr/sbin/ntpq -p | grep atlas1 | sed '{:again;s/  / /;t again}' | cut -d ' ' -f 9";
    if(root==NULL)
        cmd="/usr/sbin/ntpq -c rv | grep offset | awk '{print $1}'";
    else{
        cmd = (char*)malloc( 512 );
        sprintf(cmd, "/usr/sbin/ntpq -c rv %s | grep offset | awk '{print $1}'", root);
    }

    char line[512];
    int line_len = 512;

    FILE * fp = popen( cmd, "r" );
    if( fp == NULL ) {
        perror( cmd );
        offset=0;
    }
    if( fgets(line, line_len, fp ) == NULL ) {
        perror( "fgets()");
        offset=0;
    }
    if( sscanf( line, "offset=%lf,", &offset ) == 0 ) {
//    if( sscanf( line, "jitter=%lf,", &offset ) == 0 ) {
//    if( sscanf( line, "%lf,", &offset ) == 0 ) {
        perror( "sscanf()");
        offset=0;
    }
    offset /= 1000.0;
}

void individual_timings(int sz, bool master, int rank, double offset, int msg_size, int report){
    double start,stop,min,max,duration,min_off;
    double * results = NULL;
    double bar_dur,bca_dur,sca_dur,gat_dur;

    int rin,rax;
    //msg_size = 128;
    void* msg = malloc( msg_size );
    void* scatter_buf;
    double * bc_results;
    double * sc_results;
    void * gt_buf;
    int results_size = sizeof(double) * 4;
    if(master){
        results = (double*)malloc( results_size );
        bc_results = (double*)malloc( sizeof(double) * sz );
        sc_results = (double*)malloc( sizeof(double) * sz );
        gt_buf = malloc( msg_size * sz );
    }


    //timing barrier()
    //this can be timed from the master because it is implemented as a broadcast and gather.
    start = get_dtime( offset );
    if( LIBI_barrier() != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i barrier failed\n", __FUNCTION__, __LINE__, rank);
    if(master){
        stop = get_dtime( offset );
        duration = stop-start;
        bar_dur=duration;
        //fprintf(stderr, "barrier\t%lf\n", duration);
    }
    sleep(1);

    //timing broadcast
    start = get_dtime( offset );
    if( LIBI_broadcast(msg, msg_size) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i broadcast failed\n", __FUNCTION__, __LINE__, rank);
    stop = get_dtime( offset );
    if( LIBI_gather( (void*)&stop, sizeof(double), (void*)bc_results) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i gathering broadcast results failed\n", __FUNCTION__, __LINE__, rank);
    if(master){
        get_range(bc_results, sz, min, max, rin, rax);
        duration = max-start;
        min_off = min-start;
        bca_dur=duration;
        //fprintf(stderr, "bcast\t%lf\t%lf\n", duration, min_off);
    }
    sleep(1);

    //timing scatter
    start = get_dtime( offset );
    if(master)
        scatter_buf=malloc( sz * msg_size );
    if( LIBI_scatter(scatter_buf, msg_size, msg) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i scatter failed\n", __FUNCTION__, __LINE__, rank);
    stop = get_dtime( offset );
    if( LIBI_gather( (void*)&stop, sizeof(double), (void*)sc_results) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i gathering scatter results failed\n", __FUNCTION__, __LINE__, rank);
    if(master){
        get_range(sc_results, sz, min, max, rin, rax);
        duration = max-start;
        min_off = min-start;
        results[2]=duration;
        sca_dur=duration;
        //fprintf(stderr, "scatter\t%lf\t%lf\n", duration, min_off);
    }
    sleep(1);

    //timing gather, broadcast a startime, wait until that time, gather
    start = get_dtime( offset );
    start += 2.0;  //2 secs from now
    if( LIBI_broadcast( (void*)&start, sizeof(double) ) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i broadcasting start time failed\n", __FUNCTION__, __LINE__, rank);

    double n = get_dtime(offset);
    while( start > n ){  //wait untill start time
        n = get_dtime(offset);
    }

    if( LIBI_gather((void*)msg, msg_size, (void*)gt_buf) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i gathering failed\n", __FUNCTION__, __LINE__, rank);
    if(master){
        stop = get_dtime( offset );
        duration = stop-start;
        results[3]=duration;
        gat_dur=duration;
        //fprintf(stderr, "gather\t%lf\n", duration);
    }

    if(report){
        char result_str[512];
        sprintf(result_str, "%i\t%f\t%f\t%f\t%f", msg_size, bar_dur, bca_dur, sca_dur, gat_dur);
        results_size = strlen(result_str)+1;
        if( LIBI_sendUsrData( (void*)result_str, &results_size ) != LIBI_OK ){
            if(master)
                fprintf(stderr, "%s(%i) could not send results to parent\n", __FUNCTION__, __LINE__);
        }
    }
}

int main(int argc, char** argv) {
    libi_rc_e rc;
    int rank;
    int sz;
    bool master = false;
    void* msg;
    int msg_size;

    if(LIBI_init(1, &argc, &argv) != LIBI_OK){
        fprintf(stderr, "failied member init\n");
        return (EXIT_FAILURE);
    }
    rc = LIBI_getSize(&sz);
    if(LIBI_amIMaster() == LIBI_YES){
        master = true;
    }
    rc = LIBI_getMyRank(&rank);
    if(rc != LIBI_OK)
        fprintf(stderr, "failed to get rank.\n");

//    //thesis timing
//    double offset;
//    get_offset(offset, NULL);
//    //offset=0;
//
//    LIBI_barrier();
//    sleep(1);
//    individual_timings(sz, master, rank, offset, sizeof(int), true );
    

    //round trip timing
    msg_size=sizeof(int);
    int rcv_size=msg_size;
    if(master)
        rcv_size = msg_size * sz;
    void* recv=malloc( rcv_size );

    double start,stop;
    double bar_dur,bca_dur,sca_dur;
    msg = malloc( msg_size );

    //timing barrier()
    //this can be timed from the master because it is implemented as a 1 byte broadcast and gather.
    if(master)
        start = get_dtime( 0 );
    if( LIBI_barrier() != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i barrier failed\n", __FUNCTION__, __LINE__, rank);
    if(master){
        stop = get_dtime( 0 );
        bar_dur = stop-start;
    }
    sleep(1);

    //timing broadcast/gather
    if(master)
        start = get_dtime( 0 );
    if( LIBI_broadcast( msg, msg_size ) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i broadcast failed\n", __FUNCTION__, __LINE__, rank);
    if( LIBI_gather( msg, msg_size, (void*)recv) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i gathering broadcast results failed\n", __FUNCTION__, __LINE__, rank);
    if(master){
        stop = get_dtime( 0 );
        bca_dur = stop - start;
    }
    sleep(1);

    //timing scatter/gather
    if(master)
        start = get_dtime( 0 );
    if( LIBI_scatter( recv, msg_size, msg ) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i broadcast failed\n", __FUNCTION__, __LINE__, rank);
    if( LIBI_gather( msg, msg_size, recv ) != LIBI_OK )
        fprintf(stderr, "%s(%i) rank:%i gathering broadcast results failed\n", __FUNCTION__, __LINE__, rank);
    if(master){
        stop = get_dtime( 0 );
        sca_dur = stop - start;
    }

    //send results to front end
    char result_str[512];
    int results_size=0;
    if(master){
        sprintf(result_str, "%i\t%f\t%f\t%f", msg_size, bar_dur, bca_dur, sca_dur);
        results_size = strlen(result_str)+1;
    }
    if( LIBI_sendUsrData( (void*)result_str, &results_size ) != LIBI_OK ){
        if(master)
            fprintf(stderr, "%s(%i) could not send results to front end\n", __FUNCTION__, __LINE__);
    }





    //verification
//    if( LIBI_recvUsrData((void**)&msg, &msg_size) != LIBI_OK )
//        fprintf(stderr, "Failed to receive data from FE.\n");
//
//    LIBI_getSize(&sz);
//    char* sendbuf;
//    char* cur;
//    if( master){
//        char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
//        sendbuf = (char*)malloc( sz );
//        cur = sendbuf;
//        int copy_size = sz;
//        while( copy_size >= 26 ){
//            memcpy( cur, alphabet, 26 );
//            cur+=26;
//            copy_size-=26;
//        }
//        if( copy_size > 0 )
//            memcpy( cur, alphabet, copy_size );
//    }
//
//    char my_char='x';
//    char temp[15];
//    int combo_size;
//    char* combo_str = NULL;
//
//    if( LIBI_scatter( sendbuf, 1, &my_char ) != LIBI_OK )
//        fprintf(stderr, "scatter failed.\n");
//
//    sprintf(temp,"%i", sz-1);
//    if( master )
//        combo_size = 1 + strlen( temp );
//    if( LIBI_broadcast( &combo_size, sizeof(int) ) != LIBI_OK )
//        fprintf(stderr, "broadcast failed.\n");
//
//    char* combo = (char*)malloc( combo_size + 1 );
//    char* begining = "%c%0";
//    char format[15];
//    sprintf(format, "%s%ii", begining, combo_size-1);
//    sprintf(combo, format, my_char, rank);
//    fprintf(stderr, "rank:%i char:%c combo:%s\n", rank, my_char, combo);
//
//    combo_str = (char*)malloc( (sz * combo_size) + 1);
//    if ( LIBI_gather(combo, combo_size, combo_str) != LIBI_OK )
//        fprintf(stderr, "Could not gather\n");
//
//    if(master){
//        combo_str[sz*combo_size]='\0';
//        fprintf(stderr, "---combo_str:%s\n", combo_str);
//        cur = combo_str;
//        bool correct = true;
//        for(int i = 0; i < sz && correct; i++){
//            if( cur[0] != sendbuf[i] )
//                correct = false;
//            cur++;
//            if( atoi(cur) != i )
//                correct = false;
//            cur += combo_size-1;
//        }
//        if( correct )
//            fprintf(stderr, "Successfull Communicaiton.\n");
//        else
//            fprintf(stderr, "Communicaiton Failure.\n");
//    }
//
//    char done = 'x';
//    int done_len = 1;
//    if( LIBI_sendUsrData( &done, &done_len ) != LIBI_OK )
//        fprintf(stderr, "Send to FE.\n");

    LIBI_finalize();

    return 0;
}


void checkEnv(char* name, std::ofstream& fout){
    char* test = getenv(name);
    if(test!=NULL)
        fout << getpid() << ": $" << name << "=" << test << endl;
    else
        fout << getpid() << ": could not find $" << name << endl;
}



