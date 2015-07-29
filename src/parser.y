%{
/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                 Detailed MRNet usage rights in "LICENSE" file.           *
 ****************************************************************************/

#if defined(os_windows)
#include <malloc.h>
#else
#include <stdlib.h>
#endif

#include "mrnet/Types.h"
#include "ParsedGraph.h"
#include <list>

static std::list <MRN::ParsedGraph::Node *> hostlist;
static std::list <MRN::ParsedGraph::Node *> potential_root;

int yylex(void);
#define YYMALLOC ::malloc
#define YYFREE ::free

namespace MRN
{

#if defined(__cplusplus)
extern "C" {
void yyerror(const char *s);
}
#endif

#define YYDEBUG 1
extern int lineNum;

#if YYBYACC			/* Make sure byacc defines MRN::mrnparse */
int mrnparse();
#ifdef yyparse
#undef yyparse
#endif
#define yyparse MRN::mrnparse
#endif

%}

%union {
    unsigned int uval;
    char * hostname;
    MRN::ParsedGraph::Node * node_ptr;
}

%token <hostname> HOSTNAME
%token <uval> MRN_UINT
%token COLON SEMI ARROW
%token STAR
%type <node_ptr> host

%%
config: line config
| line
{
    if(potential_root.size() != 1){
        fprintf(stderr, "graph is not connected, found %" PRIszt " potential roots\n",
                potential_root.size());
        YYABORT;
    }
    parsed_graph->set_Root( *potential_root.begin() );
    potential_root.clear();
    hostlist.clear();
}
;

line: host ARROW hosts SEMI
{
    std::list<ParsedGraph::Node *>::iterator iter = hostlist.begin();
    for(; iter != hostlist.end(); iter++){
        ParsedGraph::Node * cur_node;
        cur_node = (*iter);
        potential_root.remove(cur_node); //node cannot be a root
        $1->add_Child(cur_node);
        cur_node->set_Parent( $1 );
    }
    hostlist.clear();
}
| host SEMI
| error
{
    fprintf(stderr, "line parse error on line %d\n", lineNum-1);
    YYABORT;
}
;

hosts: hosts host
{
    hostlist.push_back($2);
}
| host
{
    hostlist.push_back($1);
}
;


host: HOSTNAME COLON MRN_UINT
{
    if( !parsed_graph ){
        hostlist.clear(); 
        potential_root.clear();
        parsed_graph = new ParsedGraph;
    }

    ParsedGraph::Node * cur_node = parsed_graph->find_Node($1, $3);
    if(cur_node == NULL){
        cur_node = new ParsedGraph::Node($1, $3);
        free($1);
        parsed_graph->add_Node(cur_node);
        potential_root.push_back(cur_node);
    }
    $$ = cur_node;
}
| error
{
    fprintf(stderr, "host parse error on line %d\n", lineNum);
    YYABORT;
}
;

%%

void yyerror(const char * /* s */)
{
    //fprintf(stderr, "%s\n", s);
}

} // namespace MRN

#if YYBYACC			/* Make sure byacc uses the MRN namespace */
using namespace MRN;
#endif
