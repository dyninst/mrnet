/****************************************************************************
 *  Copyright © 2011 The Regents of the University of New Mexico            *
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
#include <map>
#include <set>
#include <queue>
#include <sys/time.h>

using namespace std;

const float REMOTE_LAUNCH = 0.257;  //from file system
//const float REMOTE_LAUNCH = 0.117;  //from cache
const float LOCAL_LAUNCH = 0.029;
const float SEQUENTIAL_WAIT = 0.007;  //0.009
const float PRE_LAUNCH = 0.02;  //0.009

#define MAX_CHILDREN_RESOURCE_LIMIT 1000

typedef struct _topo_node {
    string hn;                       //hostname
    vector<_topo_node *> children;   //children to launch, in order from begin() to end()
} topo_node;

typedef struct _possible_position {
    double start_time;
    int breadth;
    int depth;
    topo_node* parent;
} possible_position;

struct priority_order{
  bool operator()(possible_position* p1, possible_position* p2) const
  {
    return (p1->start_time > p2->start_time);
  }
};

struct list_order{
  bool operator()(possible_position* p1, possible_position* p2) const
  {
    return (p1->start_time < p2->start_time);
  }
};

const int sequential = -2;
const int recursive = -1;
const int greedy = 0;

double launch_model( int type, int num, int & breadth, int & depth, string* & topo, double remote_time, double sequential_time, double pre_launch_time );
double recursive_launch( possible_position* pos, int& i, int num, int & breadth, int & depth, double remote_time, double sequential_time);

int main(int argc, char** argv) {

    double r_t=REMOTE_LAUNCH;
    double s_t=SEQUENTIAL_WAIT;
    double p_t=PRE_LAUNCH;
    if(argc > 1)
        r_t = atof( argv[1] );
    if(argc > 2)
        s_t = atof( argv[2] );
    if(argc > 3)
        p_t = atof( argv[3] );

    vector<int> trees;
    vector<int> nodes;
    set< pair<int,int> > validate;
    int n,t;
//    int num_trees = 0;
//    int num_nodes;
//    int* trees;
//    int* nodes;
    if( argc > 4 ){
        FILE* fin = fopen( argv[4], "r" );
        char line[512];
        int line_len = 512;

        if( fin == NULL ) {
            fprintf(stdout, "could not open file: %f\n", argv[4]);
        }

        bool readTrees=true;
        while( fgets(line, line_len, fin ) != NULL ) {
            fprintf(stdout, "%s", line);
            if( line[0]=='|' )
                readTrees=false;
            else if(line[0]=='*'){
                if( sscanf(line, "*%i\t%i", &n, &t) == 2 )
                    validate.insert( pair<int,int>(n,t) );
            }else{
                if( readTrees )
                    trees.push_back( atoi( line ) );
                else
                    nodes.push_back( atoi( line ) );
            }
        }
    }
    if(trees.size() == 0){
        int k;
        for(k=-2; k <= 16; k++)
            trees.push_back( k );
        trees.push_back( 32 );
        trees.push_back( 64 );
        trees.push_back( 128 );
    }
    if(nodes.size() == 0){
        nodes.push_back( 15 );
        nodes.push_back( 31 );
        nodes.push_back( 65 );
        nodes.push_back( 127 );
        nodes.push_back( 255 );
        nodes.push_back( 511 );
        nodes.push_back( 1025 );
        nodes.push_back( 2047 );
        nodes.push_back( 4095 );
        nodes.push_back( 8191 );
        nodes.push_back( 16383 );
        nodes.push_back( 32767 );
        nodes.push_back( 65535 );
        nodes.push_back( 131071 );

//        num_trees = 22;
//        num_nodes = 14;
//        trees = (int*)malloc(sizeof(int)*num_trees);
//        int k;
//        for(k=-2; k <= 16; k++)
//            trees[k+2] = k;
//        trees[19]=32;
//        trees[20]=64;
//        trees[21]=128;
//        nodes = (int*)malloc(sizeof(int)*num_nodes);
//        k=0;
//        nodes[k++]=15;
//        nodes[k++]=31;
//        nodes[k++]=65;
//        nodes[k++]=127;
//        nodes[k++]=255;
//        nodes[k++]=511;
//        nodes[k++]=1025;
//        nodes[k++]=2047;
//        nodes[k++]=4095;
//        nodes[k++]=8191;
//        nodes[k++]=16383;
//        nodes[k++]=32767;
//        nodes[k++]=65535;
//        nodes[k++]=131071;
    }

    int breadth,depth;
    ostringstream bd;
    ostringstream validation;
    ostringstream creation_times;


    fprintf(stdout, "using remote time of: %f\n", r_t);
    fprintf(stdout, "using sequential time of: %f\n", s_t);
    fprintf(stdout, "using pre-launch time of: %f\n", p_t);
    fprintf(stdout, "N\t");

    vector<int>::iterator t_iter;
    for(t_iter = trees.begin(); t_iter != trees.end(); t_iter++){
        if( (*t_iter) == sequential ){
            fprintf(stdout, "seq\t");
        }else if( (*t_iter) == recursive ){
            fprintf(stdout, "rec\t");
        } else if( (*t_iter) == greedy ){
            fprintf(stdout, "gre\t");
        } else {
            fprintf(stdout, "%i\t", (*t_iter) );
        }
    }
    fprintf(stdout, "\n");

    timeval t1,t2;
    double diff;
    
    vector<int>::iterator n_iter;
    for(n_iter = nodes.begin(); n_iter != nodes.end(); n_iter++){
        fprintf(stdout, "%i\t", (*n_iter) );
        creation_times << (*n_iter) << "\t";
        for(t_iter=trees.begin(); t_iter != trees.end(); t_iter++){
            double temp;
            string* temp_topo = NULL;

            if( validate.find( pair<int,int>(*n_iter, *t_iter) ) != validate.end() )
                temp_topo = new string();

            gettimeofday( &t1, NULL );
            temp = launch_model( (*t_iter), (*n_iter), breadth, depth, temp_topo, r_t, s_t, p_t );
            gettimeofday( &t2, NULL );
            
            diff = ((double)t2.tv_sec + ((double)t2.tv_usec * 0.000001)) - ((double)t1.tv_sec + ((double)t1.tv_usec * 0.000001));
            creation_times << temp << "\t";

            fprintf(stdout, "%.3f\t", temp);
            bd << (*n_iter) << "\t" << (*t_iter) << "\t" << breadth << "\t" << depth << "\n";

            if(temp_topo!=NULL)
                validation << (*n_iter) << "\t" << (*t_iter) << "\t" << *temp_topo << "\n";

        }
        fprintf(stdout, "\n");
        creation_times << "\n";
    }

    fprintf(stdout, "\nN\tCreation times\n%s", creation_times.str().c_str() );
    fprintf(stdout, "\nN\ttype\tbreadth\tdepth\n%s", bd.str().c_str() );
    fprintf(stdout, "\nN\ttype\ttopology\n%s", validation.str().c_str() );

    return 0;
}

void extract_topo( topo_node* node, string & topo){
    string ret;
    vector<topo_node*>::iterator child;

    topo = node->hn;
    if( !node->children.empty() ){
        topo += "{";
        child = node->children.begin();
        while( child != node->children.end() ){
            ret.clear();
            extract_topo( (*child), ret );
            topo += ret + "," ;
            child++;
        }
        topo.erase( topo.size() - 1 );
        topo += "}";
    }
}

double launch_model( int type, int num, int & breadth, int & depth, string* & topo, double remote_time, double sequential_time, double pre_launch_time ){

    possible_position* pos;
    topo_node* root;
    topo_node* cur_node;
    int i;
    double max_time=0;
    char temp_hn[50];

    breadth = 0;
    depth = 0;
    root = new topo_node;
    root->hn = "0";

    if( type == greedy || type < sequential ){
        priority_queue<possible_position*, vector<possible_position*>, priority_order> possible_positions; //sorted by launch_time
        pos = (possible_position*)malloc( sizeof(possible_position) );
        pos->start_time = pre_launch_time + remote_time;
        pos->parent = root;
        pos->depth = 1;
        pos->breadth = 0;
        possible_positions.push( pos );

        for( i = 1; i < num; i++ ){
            //get position with fastest start time
            pos = possible_positions.top();
            possible_positions.pop();

            if(max_time < pos->start_time){
                max_time = pos->start_time;
                breadth = pos->breadth;
                depth = pos->depth;
            }

            cur_node = new topo_node;
            sprintf(temp_hn, "%i_%i:%i", i, pos->breadth, pos->depth);
            cur_node->hn = string( temp_hn );

            pos->parent->children.push_back( cur_node );
            
            //add the child of current node
            possible_position* descendant = (possible_position*)malloc( sizeof(possible_position) );
            descendant->start_time = pos->start_time + remote_time;
            descendant->parent = cur_node;
            descendant->breadth = pos->breadth;
            descendant->depth = pos->depth + 1;
            possible_positions.push( descendant );

            //add the sibling of currrent node
            pos->start_time += sequential_time;
            pos->breadth++;
            if( pos->parent->children.size() < MAX_CHILDREN_RESOURCE_LIMIT )
                possible_positions.push( pos );

        }
    } else if( type == recursive ){
        pos = (possible_position*)malloc( sizeof(possible_position) );
        pos->start_time = pre_launch_time + remote_time;
        pos->parent = root;
        depth++;

        int c=1;
        max_time = recursive_launch( pos, c, num-1, breadth, depth, remote_time, sequential_time);
    } else if( type == sequential ){
        if( num > 0 ){
            max_time = pre_launch_time + remote_time;
            depth++;
        }
        for( i = 1; i < num; i++ ){
            cur_node = new topo_node;
            sprintf(temp_hn, "%i_%i:%i", i, breadth, depth);
            cur_node->hn = string( temp_hn );

            root->children.push_back( cur_node );
            if( i+1 < num ){
                breadth++;
                max_time += sequential_time;
            }
        }

    } else if( type > greedy ){
        //k-ary tree
        queue<possible_position*> available_positions;
        possible_position* cur_pos = (possible_position*)malloc( sizeof(possible_position) );
        possible_position* temp;
        
        cur_pos->parent = root;
        cur_pos->start_time = pre_launch_time + remote_time;
        cur_pos->breadth = 0;
        cur_pos->depth = 1;

        for( i = 1; i < num; i++ ){
            if( cur_pos->parent->children.size() >= type  ){
                free( cur_pos );
                cur_pos = available_positions.front();
                available_positions.pop();
                cur_pos->start_time += remote_time; //used for children's start time;
                cur_pos->depth++;
            }

            cur_node = new topo_node;
            sprintf(temp_hn, "%i_%i:%i", i, cur_pos->breadth, cur_pos->depth);
            cur_node->hn = string( temp_hn );

            temp = (possible_position*)malloc( sizeof(possible_position));
            temp->parent = cur_node;
            temp->start_time = cur_pos->start_time;
            temp->breadth = cur_pos->breadth;
            temp->depth = cur_pos->depth;
            if(max_time < temp->start_time){
                max_time = temp->start_time;
                breadth = temp->breadth;
                depth = temp->depth;
            }

            cur_pos->parent->children.push_back( cur_node );
            cur_pos->start_time += sequential_time;
            cur_pos->breadth++;

            available_positions.push( temp );
        }
    }

    if( topo != NULL )
        extract_topo( root, *topo );
    return max_time;
}


double recursive_launch( possible_position* pos, int& i, int num, int & breadth, int & depth, double remote_time, double sequential_time){
    topo_node* cur_node;
    char temp_hn[50];
    if( num <= 0 ){
        return pos->start_time;
    } else {
        int mine = (int)floor(num / 2);
        int theirs = num - mine;
        int their_b = breadth;
        int their_d = depth;
        int my_b = breadth;
        int my_d = depth;
        double max1=0;
        double max2=0;

        //theirs first
        if(theirs>0){
            cur_node = new topo_node;
            sprintf(temp_hn, "%i_%i:%i", i++, breadth, depth);
            cur_node->hn = string( temp_hn );
            pos->parent->children.push_back( cur_node );
            theirs--;
            their_d++;

            if(theirs>0){
                possible_position* temp = (possible_position*)malloc( sizeof(possible_position));
                temp->parent = cur_node;
                temp->start_time = pos->start_time+remote_time;
                max1 = recursive_launch(temp, i, theirs, their_b, their_d, remote_time, sequential_time);
            } else
                max1 = pos->start_time;
        }

        if(mine>0){
            my_b++;
            pos->start_time += sequential_time;
            max2 = recursive_launch(pos, i, mine, my_b, my_d, remote_time, sequential_time);
        }

        if( max1 > 0 && max1 > max2 ){
            breadth = their_b;
            depth = their_d;
            return max1;
        } else {
            breadth = my_b;
            depth = my_d;
            return max2;
        }
    }
}

