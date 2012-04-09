/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "xplat/xplat_utils.h"

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

#include "mrnet/Tree.h"
#include "xplat/NetUtils.h"

static void usage_exit(char* program);

void init_local()
{
#ifdef os_windows
	WORD version = MAKEWORD(2,2);
	WSADATA data;
	if (WSAStartup(version, &data) != 0)
		fprintf(stderr, "WSAStartup failed!\n");
#endif	
}

int main(int argc, char **argv)
{
    std::string hosts_file, fe_host, be_hosts_file, cp_hosts_file,
                output_file, topology, topology_type;
    bool have_hosts = false, have_fe_host = false;
    bool have_be_hosts = false, have_cp_hosts = false;
    bool have_max = false, have_be_max = false, have_cp_max = false;
    bool have_topology = false;

    int c;
    int max_procs=1024; // bigger than any reasonable use
    int be_max_procs=1024; // bigger than any reasonable use
    int cp_max_procs=1024; // bigger than any reasonable use

    FILE *infile, *outfile;

    if( argc == 1 ) usage_exit(argv[0]);

    extern char * optarg;
    const char optstring[] = "b:c:f:h:o:p:q:r:t:";
    while (true) {

#if defined(os_linux)
        int option_index = 0;
        static struct option long_options[] = {
            {"behosts", 1, 0, 'b'},
            {"cphosts", 1, 0, 'c'},
            {"fehost", 1, 0, 'f'},
            {"hosts", 1, 0, 'h'},
            {"outfile", 1, 0, 'o'},
            {"maxprocs", 1, 0, 'p'},
            {"beprocs", 1, 0, 'q'},
            {"cpprocs", 1, 0, 'r'},
            {"topology", 1, 0, 't'},
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
            have_be_hosts = true;
            be_hosts_file = optarg;
            break;
        case 'c':
            have_cp_hosts = true;
            cp_hosts_file = optarg;
            break;
        case 'f':
            have_fe_host = true;
            fe_host = optarg;
            break;
        case 'h':
            have_hosts = true;
            hosts_file = optarg;
            break;
        case 'o':
            output_file = optarg;
            break;
        case 'p':
            have_max = true;
            max_procs = atoi(optarg);
            if( max_procs <= 0 )
                max_procs = 1;
            break;
        case 'q':
            have_be_max = true;
            be_max_procs = atoi(optarg);
            if( be_max_procs <= 0 )
                be_max_procs = 1;
            break;
        case 'r':
            have_cp_max = true;
            cp_max_procs = atoi(optarg);
            if( cp_max_procs <= 0 )
                cp_max_procs = 1;
            break;
        case 't':
            have_topology = true;
            if( 0 == strncmp(optarg,"b:",2) ) {
                topology_type = "balanced";
                topology = optarg+2;
            }
            else if( 0 == strncmp(optarg,"g:",2) ) {
                topology_type = "generic";
                topology = optarg+2;
            }
            else if( 0 == strncmp(optarg,"k:",2) ) {
                topology_type = "knomial";
                topology = optarg+2;
            }
            else {
                fprintf(stderr, "Error: invalid topology specification '%s'\n", optarg);
                usage_exit(argv[0]);
            }
            break;
        default:
            usage_exit(argv[0]);
        }
    }

    // sanity checks
    if( have_hosts ) {
        if( have_be_hosts || have_cp_hosts ) {
            fprintf(stderr, "Error: cannot specify --hosts and --behosts,--cphosts\n");
            usage_exit(argv[0]);
        }
        if( have_be_max || have_cp_max ) {
            fprintf(stderr, "Error: cannot specify --hosts and --beprocs,--cpprocs, perhaps you want --maxprocs??\n");
            usage_exit(argv[0]);
        }
    }
    if( have_max && (have_be_hosts || have_cp_hosts) ) {
        fprintf(stderr, "Error: cannot specify --maxprocs and --behosts,--cphosts\n");
        usage_exit(argv[0]);
    }

    if( ! have_topology ) {
        fprintf(stderr, "Error: --topology option must be provided\n");
        usage_exit(argv[0]);
    }
   
    init_local();

    // read input hosts file(s)
    std::list< std::pair<std::string, unsigned> > hosts, cp_hosts, be_hosts;
    if( ! (have_hosts || have_be_hosts) ){
        infile = stdin;
        MRN::Tree::get_HostsFromFileStream(infile, hosts);
        have_hosts = true;
    }
    else {
        if( have_hosts ) {
            infile = fopen(hosts_file.c_str(), "r");
            if( !infile ){
                fprintf(stderr, "Error: open of %s failed with '%s'\n", 
                        hosts_file.c_str(), strerror(errno) );
                exit(-1);
            }
            MRN::Tree::get_HostsFromFileStream(infile, hosts);
        }
        else {
            if( have_cp_hosts ) {
                infile = fopen(cp_hosts_file.c_str(), "r");
                if( !infile ){
                    fprintf(stderr, "Error: open of %s failed with '%s'\n", 
                            cp_hosts_file.c_str(), strerror(errno) );
                    exit(-1);
                }
                MRN::Tree::get_HostsFromFileStream(infile, cp_hosts);
            }
            if( have_be_hosts ) {
                infile = fopen(be_hosts_file.c_str(), "r");
                if( !infile ){
                    fprintf(stderr, "Error: open of %s failed with '%s'\n", 
                            be_hosts_file.c_str(), strerror(errno) );
                    exit(-1);
                }
                MRN::Tree::get_HostsFromFileStream(infile, be_hosts);
            }
        }
    }

    // set fe to local host if not given on command line
    if( ! have_fe_host )
        XPlat::NetUtils::GetLocalHostName( fe_host );

    // generate tree
    MRN::Tree * tree=NULL;

    if( topology_type == "balanced" ) {
        //fprintf(stderr, "Creating balanced topology \"%s\" from \"%s\" to \"%s\"\n",
                //topology.c_str(),
                //( hosts_file == "" ? "stdin" : hosts_file.c_str() ),
                //( output_file == "" ? "stdout" : output_file.c_str() ) );
        if( have_hosts )
            tree = new MRN::BalancedTree( topology, fe_host, hosts, max_procs );
        else
            tree = new MRN::BalancedTree( topology, fe_host, be_hosts, cp_hosts,
                                          be_max_procs, cp_max_procs );
    }
    else if( topology_type == "knomial" ) {
        //fprintf(stderr, "Creating k-nomial topology \"%s\" from \"%s\" to \"%s\"\n",
                //topology.c_str(),
                //( hosts_file == "" ? "stdin" : hosts_file.c_str() ),
                //( output_file == "" ? "stdout" : output_file.c_str() ) );
        if( have_hosts )
            tree = new MRN::KnomialTree( topology, fe_host, hosts, max_procs );
        else
            tree = new MRN::KnomialTree( topology, fe_host, be_hosts, cp_hosts,
                                         be_max_procs, cp_max_procs );
    }
    else if( topology_type == "generic" ) {
        //fprintf(stderr, "Creating generic topology \"%s\" from \"%s\" to \"%s\"\n",
                //topology.c_str(),
                //( hosts_file == "" ? "stdin" : hosts_file.c_str() ),
                //( output_file == "" ? "stdout" : output_file.c_str() ) );
        if( have_hosts )
            tree = new MRN::GenericTree( topology, fe_host, hosts, max_procs );
        else
            tree = new MRN::GenericTree( topology, fe_host, be_hosts, cp_hosts,
                                         be_max_procs, cp_max_procs );
    }
    else {
        assert(!"internal error: unknown topology");
        exit(-1);
    }

    if( tree->is_Valid() ) {

        // set topology output file
        if( output_file.empty() ){
            outfile = stdout;
        }
        else{
            outfile = fopen(output_file.c_str(), "w");
            if( !outfile ){
                fprintf(stderr, "Error: open of %s for writing failed with '%s'\n", 
                        output_file.c_str(), strerror(errno) );
                exit(-1);
            }
        }

        tree->create_TopologyFile( outfile );
    }
    else {
        fprintf(stderr, "Generated tree is not valid. Please check your topology specification and options.\n");
        return -1;
    }
    return 0;
}



void usage_exit(char* program)
{
    fprintf( stderr, "\nUsage: %s [OPTION ...] TOPOLOGY_SPEC\n\n"

             "Create a MRNet topology from the host list(s) given on the command line\n"
             "(or standard input), and writes output to a file given on the command line\n"
             "(or standard output).\n\n"

             "The format of the input host list(s) is one machine specification per line,\n"
             "where each specification is of the form \"host[:num-processors]\".\n\n"

             "\t OPTIONS:\n\n"

             "\t -b be-hosts-file, --behosts=be-hosts-file\n"
             "\t\t Specify the name of a host list file containing back-end hosts.\n"
             "\t\t This option cannot be used in combination with --hosts.\n\n"

             "\t -c cp-hosts-file, --cphosts=cp-hosts-file\n"
             "\t\t Specify the name of a host list file containing internal hosts.\n"
             "\t\t This option cannot be used in combination with --hosts.\n\n"

             "\t -f fe-host, --fehost=fe-host\n"
             "\t\t Specify the name or IP address of the front-end host for the topology.\n"
             "\t\t If this option is not given, the topology will be generated assuming\n"
             "\t\t the local host should be the front-end.\n\n"

             "\t -h hosts-file, --hosts=hosts-file\n"
             "\t\t Specify the name of a host list file containing all internal and\n"
             "\t\t back-end hosts for the topology. If this option is not given, and\n"
             "\t\t --behosts is also not given, the host list will be read from \n"
             "\t\t standard input.\n\n"

             "\t -o output-file, --output=output-file\n"
             "\t\t Specify the name of the file to which the topology will be written.\n"
             "\t\t If this option is not given, the topology will be written to\n"
             "\t\t standard output.\n\n"

             "\t -p max-host-procs, --maxprocs=max-host-procs\n"
             "\t\t This option is only valid in combination with --hosts. Specify the \n"
             "\t\t maximum number of processes to place on any machine, in which case \n"
             "\t\t the number of processes allocated to a machine will be the \n"
             "\t\t minimum of its processor count and \"max-host-procs\".\n\n"

             "\t -q max-be-procs, --beprocs=max-be-procs\n"
             "\t\t This option is only valid in combination with --behosts. Specify the\n"
             "\t\t maximum number of back-end processes to place on any back-end host,\n"
             "\t\t in which case the number of processes allocated to a host will be\n"
             "\t\t the minimum of its processor count and \"max-be-procs\".\n\n"

             "\t -r max-cp-procs, --cpprocs=max-cp-procs\n"
             "\t\t This option is only valid in combination with --cphosts. Specify the\n"
             "\t\t maximum number of communication processes to place on any internal host,\n"
             "\t\t in which case the number of processes allocated to a host will be\n"
             "\t\t the minimum of its processor count and \"max-cp-procs\".\n\n"

             "\t TOPOLOGIES:\n\n"

             "\t -t b:spec, --topology=b:spec\n"
             "\t\t Create a balanced tree using \"spec\" topology specification. The specification\n"
             "\t\t is in the format F^D, where F is the fan-out (or out-degree) and D is the\n"
             "\t\t tree depth. The number of tree leaves (or back-ends) will be F^D.\n"
             "\t\t An alternative specification is FxFxF, where the fan-out at each level\n"
             "\t\t is specified explicitly and can differ between levels.\n\n"

             "\t\t Example: \"16^3\" is a tree of depth 3 with fan-out 16, with 4096 leaves.\n"
             "\t\t Example: \"2x4x8\" is a tree of depth 3 with 64 leaves.\n\n"

             "\t -t k:spec, --topology=k:spec\n"
             "\t\t Create a k-nomial tree using \"spec\" topology specification. The specification\n"
             "\t\t is in the format K@N, where K is the k-factor (>=2) and N is the total\n"
             "\t\t number of tree nodes. The number of tree leaves (or back-ends) will be\n"
             "\t\t (N/K)*(K-1).\n\n"

             "\t\t Example: \"2@128\" is a binomial tree of 128 nodes, with 64 leaves.\n"
             "\t\t Example: \"3@27\" is a trinomial tree of 27 nodes, with 18 leaves.\n\n"

             "\t -t g:spec, --topology=g:spec\n"
             "\t\t Create a generic tree using \"spec\" topology specification. The specification\n"
             "\t\t for this option is (the agreeably complicated) N:N,N,N:... where N is\n"
             "\t\t the number of children assigned to a parent, ',' distinguishes parents on \n"
             "\t\t the same level, and ':' separates the tree into levels. For cases where \n"
             "\t\t many parents on the same level are assigned the same number of children, \n"
             "\t\t the shorthand notation 'KxN' can be used to indicate K parents should have \n"
             "\t\t N children.\n\n"

             "\t\t Example: \"2:8,4\" is a tree where the root has 2 children,\n"
             "\t\t          the 1st child has 8 children, and the\n"
             "\t\t          2nd child has 4 children.\n"
             "\t\t Example: \"2:2x6\" is a tree where the root has 2 children,\n"
             "\t\t          and the root's children each have 6 children\n"
             , program );
    exit(-1);
}
