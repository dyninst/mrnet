/****************************************************************************
 *  Copyright � 2011 The Regents of the University of New Mexico            *
 *  All Rights Reserved                                                     *
 *  Created by Josh Goehner and Dorian Arnold                               *
 *  Department of Computer Science                                          *
 *  Detailed usage rights in "LICENSE" file.                                *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "libi/libi_api.h"
#include <sys/time.h>

using namespace std;
host_dist_t* createHostDistFromFile(string filepath);
host_dist_t* createHostDistFake(int num);
host_dist_t* createHostDistSlurmBatch( int limit );
host_dist_t* createHostDistFullSlurmBatch( int limit, int pernode );

int main(int argc, char** argv) {
    host_dist_t* front;
    host_dist_t* second;
    int proc_count=-1;
    char* proc;
    timeval t1,t2,t3,t4,t5,t6;

    proc = "/g/g20/groves6/mrnet/mrnet_libi_4.0/build/x86_64-unknown-linux-gnu/tests/testmember";
    int tree_type=0;
    char* tree = getenv("LIBI_TREE_TYPE");
    if( tree != NULL )
        tree_type=atoi(tree);
    
    if(argc > 1)
        if(strcmp(argv[1],"-slurm") == 0){
            if(argc > 2)
                proc_count = atoi( argv[2] );

            front = createHostDistSlurmBatch( proc_count );
        }else if(strcmp(argv[1],"-8slurm") == 0){
            if(argc > 2)
                proc_count = atoi( argv[2] );
            if(argc > 3)
                proc = strdup( argv[3] );

            front = createHostDistFullSlurmBatch( proc_count, 8 );
        }
        else{
            proc_count = atoi(argv[1]);
            fprintf( stderr, "%s(%i) localhost %i\n", __FUNCTION__, __LINE__, proc_count );
            front = createHostDistFake( proc_count );
        }
    else{
        fprintf( stderr, "%s(%i) localhost 2\n", __FUNCTION__, __LINE__);
        front = createHostDistFake(2);
    }

    if(LIBI_fe_init(1) != LIBI_OK){
        fprintf( stderr, "%s(%i) failied init", __FUNCTION__, __LINE__);
        return (EXIT_FAILURE);
    }

    libi_sess_handle_t sess1;
    libi_sess_handle_t sess2;
    if(LIBI_fe_createSession(sess1) != LIBI_OK){
        fprintf( stderr, "%s(%i) failied create session", __FUNCTION__, __LINE__);
        return (EXIT_FAILURE);
    }

/*
    libi_env_t* e1 = (libi_env_t*)malloc(sizeof(libi_env_t));
    e1->name = "LIBI_SESSION_ORDER";
    e1->value = "1";
    e1->next = (libi_env_t*)malloc(sizeof(libi_env_t));
    (e1->next)->name = "LIBI_TEST";
    (e1->next)->value = "true";
    (e1->next)->next = NULL;
*/

/*
    if(LIBI_fe_addToSessionEnvironment(sess1, e1) != LIBI_OK){
        fprintf( stderr, "failied create session", __FUNCTION__, __LINE__);
        return (EXIT_FAILURE);
    }
*/
//    fprintf(stderr, "is this working\n");

    //char* proc = "/bin/env";
    char* args1[] = {"first", NULL};

    proc_dist_req_t first[2];
    first[0].sessionHandle = sess1;
    first[0].proc_path = proc;
    first[0].proc_argv = args1;
    first[0].hd = front;

    gettimeofday( &t1, NULL );
    if(LIBI_fe_launch(first, 1) != LIBI_OK){
        fprintf( stderr, "%s(%i) failied LIBI launch\n", __FUNCTION__, __LINE__);
        return (EXIT_FAILURE);
    }
    gettimeofday( &t2, NULL );

    //hostlist
    int chost;
    char** hostlist1;
    unsigned int sz;

    if(LIBI_fe_getHostlist(sess1, &hostlist1, &sz) != LIBI_OK )
        fprintf( stderr, "%s(%i) hostlist problems\n", __FUNCTION__, __LINE__);
    
    double launch = ((double)t2.tv_sec + ((double)t2.tv_usec * 0.000001)) - ((double)t1.tv_sec + ((double)t1.tv_usec * 0.000001));
    //fprintf( stdout, "Launching %i processes took %f seconds\n", sz, launch);

    void* msg = malloc( 128 );
    memset(msg, 1, 128);
    libi_rc_e rc;
    int n1;
    void* recv;

    if( LIBI_fe_sendUsrData(sess1, msg, 128 ) != LIBI_OK )
        fprintf( stderr, "%s(%i) FE could not send user data.\n", __FUNCTION__, __LINE__);

    if( LIBI_fe_recvUsrData(sess1, (void**)&recv, &n1) != LIBI_OK )
        fprintf( stderr, "%s(%i) FE could not receive user data.\n", __FUNCTION__, __LINE__);
    fprintf( stdout, "%i\t%u\t%f\t%s\n", tree_type, sz, launch, (char*)recv);

//    //broadcast at the member level
//    gettimeofday( &t3, NULL );
//    rc = LIBI_fe_sendUsrData(sess1, msg, 128 );
//    if(rc != LIBI_OK)
//        fprintf( stderr, "%s(%i) could not send user data.\n", __FUNCTION__, __LINE__);
//
//    rc = LIBI_fe_recvUsrData(sess1, (void**)&recv, &n1);
//    gettimeofday( &t4, NULL );
//    if(rc != LIBI_OK)
//        fprintf( stderr, "%s(%i) could not receive user data.\n", __FUNCTION__, __LINE__);
//
//    free( recv );
//    free( msg );
//    int msg_length = 128 * proc_count;
//    msg = malloc( msg_length );
//    memset( msg, 1, msg_length );
//
//    //scatter at the member level
//    gettimeofday( &t5, NULL );
//    rc = LIBI_fe_sendUsrData(sess1, msg, msg_length );
//    if(rc != LIBI_OK)
//        fprintf( stderr, "%s(%i) could not send user data.\n", __FUNCTION__, __LINE__);
//
//    rc = LIBI_fe_recvUsrData(sess1, (void**)&recv, &n1);
//    gettimeofday( &t6, NULL );
//    if(rc != LIBI_OK)
//        fprintf( stderr, "%s(%i) could not receive user data.\n", __FUNCTION__, __LINE__);
//
//    double launch = ((double)t2.tv_sec + ((double)t2.tv_usec * 0.000001)) - ((double)t1.tv_sec + ((double)t1.tv_usec * 0.000001));
//    double broadcast = ((double)t4.tv_sec + ((double)t4.tv_usec * 0.000001)) - ((double)t3.tv_sec + ((double)t3.tv_usec * 0.000001));
//    double scatter = ((double)t6.tv_sec + ((double)t6.tv_usec * 0.000001)) - ((double)t5.tv_sec + ((double)t5.tv_usec * 0.000001));
//
//    fprintf(stdout, "%i\t%f\t%f\t%f\n", proc_count, launch, broadcast, scatter);
//    fflush(stdout);

    //TODO: cleanup proc_dist_req_t, msg, recv
    return 0;
}

host_dist_t* createHostDistFromFile(string filepath){

    host_dist_t* hd = NULL;
    host_dist_t** cur = &hd;

    ifstream nodefile;
    nodefile.open(filepath.c_str(), ifstream::in);
    string line;

    if(nodefile.is_open()){
        while(nodefile.good()){
            (*cur) = (host_dist_t*)malloc(sizeof(host_dist_t));
            getline (nodefile,line);
            (*cur)->hostname = strdup(line.c_str());
            (*cur)->nproc = 1;
            (*cur)->next = NULL;
            cur = &(*cur)->next;
        }
        nodefile.close();
    }

    return hd;
}
host_dist_t* createHostDistSlurmBatch( int limit ){

    std::string ns = getenv("SLURM_NODELIST");
    //std::string ns = "zeus[14-16]";
    std::vector<char*> nodes;
    int f = ns.find('[');
    if(f<0){
        char* temp = (char*)malloc((ns.length()+1)*sizeof(char));
        temp = strdup(ns.c_str());
        nodes.push_back(temp);
    }
    else{
        //zeus[24-33,36-66,69-248,251-283]
        std::string base = ns.substr(0,f);
        std::string numbers = ns.substr(f+1, ns.length()-f-2); //extract numbers from base[numbers]
        std::string range;
        while(numbers.length()>0){
            f = numbers.find(',');
            if(f<0){
                range = numbers;
                numbers = "";
            }
            else{
                range = numbers.substr(0,f);
                numbers = numbers.substr(f+1);
            }

            int h = range.find('-');
            if(h<0){
                std::ostringstream node;
                node << base << range;
                char* temp = (char*)malloc((node.str().length()+1)*sizeof(char));
                temp = strdup(node.str().c_str());
                nodes.push_back(temp);
            }else{
                int begin = atoi(range.substr(0,h).c_str());
                int end = atoi(range.substr(h+1).c_str());
                for(int no = begin; no<=end; no++){
                    std::ostringstream node;
                    node << base << no;
                    char* temp = (char*)malloc((node.str().length()+1)*sizeof(char));
                    temp = strdup(node.str().c_str());
                    nodes.push_back(temp);
                }
            }
        }
    }

    host_dist_t* hd = NULL;
    host_dist_t* cur = NULL;
    int node_count = nodes.size();
    int per = 1;
    int high_count=0;
    int count=0;
    if( limit >= node_count ){
        per = (int)floor( limit / node_count );
        high_count = limit % node_count;
    }
    std::vector<char*>::iterator iter = nodes.begin();
    while(iter != nodes.end() && (count < limit || limit == -1) ){
        if(hd == NULL){
            hd = (host_dist_t*)malloc(sizeof(host_dist_t));
            cur = hd;
        }
        else{
            cur->next = (host_dist_t*)malloc(sizeof(host_dist_t));
            cur = cur->next;
        }
        cur->hostname = (*iter);
        cur->nproc = per;
        if( high_count > 0 ){
            cur->nproc++;
            high_count--;
        }
        cur->next = NULL;

        iter++;
        count++;
    }

    return hd;
}
host_dist_t* createHostDistFake(int num){
    host_dist_t* hd = (host_dist_t*)malloc(sizeof(host_dist_t));

    hd->hostname = "localhost";
    hd->nproc = num;
    hd->next = NULL;

/*
    host_dist_t* hd;
    host_dist_t** cur = (host_dist_t**)malloc(sizeof(host_dist_t*));
    
    cur = &hd;

    for(int i=0; i<num; i++){
        (*cur) = (host_dist_t*)malloc(sizeof(host_dist_t));
        (*cur)->hostname = "localhost";
        (*cur)->nproc = 1;
        (*cur)->next = NULL;
        cur = &((*cur)->next);
    }
*/

    return hd;
}

host_dist_t* createHostDistFullSlurmBatch( int limit, int pernode ){

    std::string ns = getenv("SLURM_NODELIST");
    //std::string ns = "zeus[14-16]";
    std::vector<char*> nodes;
    int f = ns.find('[');
    if(f<0){
        char* temp = (char*)malloc((ns.length()+1)*sizeof(char));
        temp = strdup(ns.c_str());
        nodes.push_back(temp);
    }
    else{
        //zeus[24-33,36-66,69-248,251-283]
        std::string base = ns.substr(0,f);
        std::string numbers = ns.substr(f+1, ns.length()-f-2); //extract numbers from base[numbers]
        std::string range;
        while(numbers.length()>0){
            f = numbers.find(',');
            if(f<0){
                range = numbers;
                numbers = "";
            }
            else{
                range = numbers.substr(0,f);
                numbers = numbers.substr(f+1);
            }

            int h = range.find('-');
            if(h<0){
                std::ostringstream node;
                node << base << range;
                char* temp = (char*)malloc((node.str().length()+1)*sizeof(char));
                temp = strdup(node.str().c_str());
                nodes.push_back(temp);
            }else{
                int begin = atoi(range.substr(0,h).c_str());
                int end = atoi(range.substr(h+1).c_str());
                for(int no = begin; no<=end; no++){
                    std::ostringstream node;
                    node << base << no;
                    char* temp = (char*)malloc((node.str().length()+1)*sizeof(char));
                    temp = strdup(node.str().c_str());
                    nodes.push_back(temp);
                }
            }
        }
    }

    host_dist_t* hd = NULL;
    host_dist_t* cur = NULL;
    int node_count = nodes.size();
    int per = pernode;
    int count=0;
    std::vector<char*>::iterator iter = nodes.begin();
    while(iter != nodes.end() && (count < limit || limit == -1) ){
        if(hd == NULL){
            hd = (host_dist_t*)malloc(sizeof(host_dist_t));
            cur = hd;
        }
        else{
            cur->next = (host_dist_t*)malloc(sizeof(host_dist_t));
            cur = cur->next;
        }
        cur->hostname = (*iter);
        cur->nproc = pernode;
        cur->next = NULL;

        iter++;
        count++;
    }

    return hd;
}

