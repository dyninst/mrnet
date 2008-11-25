/****************************************************************************
 * Copyright © 2003-2008 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(os_linux)
# include <getopt.h>
#elif !defined(os_windows)
# include <unistd.h>
#else
extern int P_getopt(int argc, char* argv[], const char* optstring);
#define getopt P_getopt
#endif

#include <list>
#include <string>

#include "mrnet/MRNet.h"

static void print_usage(char* program);

int main(int argc, char **argv)
{
    std::string machine_file="", output_file="", topology="", topology_type="";
    std::list< std::pair<std::string, unsigned> > hosts;
    int c;
    int max_procs=1024; // bigger than any reasonable use
    FILE * infile, *outfile;

    if( argc == 1 ){
        print_usage(argv[0]);
        exit(0);
    }

    extern char * optarg;
    const char optstring[] = "b:k:m:o:";
    while (true) {

#if defined(os_linux)
        int option_index = 0;
        static struct option long_options[] = {
            {"balanced", 1, 0, 'b'},
            {"knomial", 1, 0, 'k'},
            {"maxppn", 1, 0, 'm'},
            {"other", 1, 0, 'o'},
            {0, 0, 0, 0}
        };
        c = getopt_long( argc, argv, optstring, long_options, &option_index );
#else
        c = getopt( argc, argv, optstring );
#endif
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            topology_type = "balanced";
            topology = optarg;
            break;
        case 'k':
            topology_type = "knomial";
            topology = optarg;
            break;
        case 'm':
            max_procs = atoi(optarg);
            if( max_procs <= 0 )
                max_procs = 1;
            break;
        case 'o':
            topology_type = "other";
            topology = optarg;
            break;
        default:
            print_usage(argv[0]);
            break;
        }
    }

    //hack alert:if topology_type is 2nd to last arg, only infile is specified
    //           if topology_type is 3rd to last arg, outfile is also specified
    if( topology == argv[argc-2] ||
        ( std::string(argv[argc-2]).find('=') != std::string::npos ) ){
        machine_file = argv[argc-1];
    }
    else if( topology == argv[argc-3] ||
             ( std::string(argv[argc-3]).find('=') != std::string::npos ) ){
        output_file = argv[argc-1];
        machine_file = argv[argc-2];
    }
    else if( topology != argv[argc-1] ||
             ( std::string(argv[argc-1]).find('=') != std::string::npos ) ) {
        fprintf(stderr, "topology_type \"%s\" should be last, 2nd to last or 3rd to last!\n",
                topology.c_str());
        print_usage(argv[0]);
    }

    if( machine_file == "" ){
        infile = stdin;
    }
    else{
        infile = fopen(machine_file.c_str(), "r");
        if( !infile ){
            fprintf(stderr, "%s (reading):", machine_file.c_str() );
            perror("fopen()");
            exit(-1);
        }
    }
    if( output_file == "" ){
        outfile = stdout;
    }
    else{
        outfile = fopen(output_file.c_str(), "w");
        if( !outfile ){
            fprintf(stderr, "%s (writing): ", output_file.c_str() );
            perror("fopen()");
            exit(-1);
        }
    }

    MRN::Tree::get_HostsFromFileStream(infile, hosts);

    MRN::Tree * tree=NULL;

    if( topology_type == "balanced" ){
        //fprintf(stderr, "Creating balanced topology \"%s\" from \"%s\" to \"%s\"\n",
                //topology.c_str(),
                //( machine_file == "" ? "stdin" : machine_file.c_str() ),
                //( output_file == "" ? "stdout" : output_file.c_str() ) );
        tree = new MRN::BalancedTree( topology, hosts, max_procs, max_procs, max_procs );
    }
    else if( topology_type == "knomial" ){
        //fprintf(stderr, "Creating k-nomial topology \"%s\" from \"%s\" to \"%s\"\n",
                //topology.c_str(),
                //( machine_file == "" ? "stdin" : machine_file.c_str() ),
                //( output_file == "" ? "stdout" : output_file.c_str() ) );
        tree = new MRN::KnomialTree( topology, hosts, max_procs );
    }
    else if( topology_type == "other" ){
        //fprintf(stderr, "Creating generic topology \"%s\" from \"%s\" to \"%s\"\n",
                //topology.c_str(),
                //( machine_file == "" ? "stdin" : machine_file.c_str() ),
                //( output_file == "" ? "stdout" : output_file.c_str() ) );
        tree = new MRN::GenericTree( topology, hosts, max_procs );
    }
    else{
        fprintf(stderr, "Error: Unknown topology \"%s\".", topology.c_str() );
        exit(-1);
    }

    tree->create_TopologyFile( outfile );

    return 0;
}



void print_usage(char* program)
{
    fprintf( stderr, "\nUsage: %s [<OPTION>] <TOPOLOGY_SPEC> [INFILE] [OUTFILE]\n\n"

             "Create a MRNet topology from the machines listed in [INFILE],\n"
             "or standard input, and writes output to [OUTFILE], or standard output.\n\n"

             "The format of the input machine list is one machine specification per line,\n"
             "where each specification is of the form \"host[:num-processors]\".\n\n"

             "\t OPTIONS:\n\n"

             "\t -m max-host-procs, --maxprocs=max-host-procs\n"
             "\t\t Specify the maximum number of processes to place on any machine,\n"
             "\t\t in which case the number of processes allocated to a machine will be\n"
             "\t\t the minimum of its processor count and \"max-host-procs\".\n\n"

             "\t TOPOLOGIES:\n\n"

             "\t -b topology, --balanced=topology\n"
             "\t\t Create a balanced tree using \"topology\" specification. The specification\n"
             "\t\t is in the format F^D, where F is the fan-out (or out-degree) and D is the\n"
             "\t\t tree depth. The number of tree leaves (or back-ends) will be F^D.\n"
             "\t\t An alternative specification is FxFxF, where the fan-out at each level\n"
             "\t\t is specified explicitly and can differ between levels.\n\n"

             "\t\t Example: \"16^3\" is a tree of depth 3 with fan-out 16, with 4096 leaves.\n"
             "\t\t Example: \"2x4x8\" is a tree of depth 3 with 64 leaves.\n\n"

             "\t -k topology, --knomial=topology\n"
             "\t\t Create a k-nomial tree using \"topology\" specification. The specification\n"
             "\t\t is in the format K@N, where K is the k-factor (>=2) and N is the total\n"
             "\t\t number of tree nodes. The number of tree leaves (or back-ends) will be\n"
             "\t\t (N/K)*(K-1).\n\n"

             "\t\t Example: \"2@128\" is a binomial tree of 128 nodes, with 64 leaves.\n"
             "\t\t Example: \"3@27\" is a trinomial tree of 27 nodes, with 18 leaves.\n\n"

             "\t -o topology, --other=topology\n"
             "\t\t Create a generic tree using \"topology\" specification. The specification\n"
             "\t\t for this option is (the agreeably complicated) N:N,N,N:... where N is\n"
             "\t\t the number of children, ',' distinguishes nodes on the same level,\n"
             "\t\t and ':' separates the tree into levels.\n\n"

             "\t\t Example: \"2:8,4\" is a tree where the root has 2 children,\n"
             "\t\t          the 1st child has 8 children, and the\n"
             "\t\t          2nd child has 4 children.\n", program );
    exit(-1);
}
