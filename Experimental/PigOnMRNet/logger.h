// Copyright Paradyn, 2008: contact jolly@cs.wisc.edu for details

#if !defined( logger_h )
#define logger_h 1

#include <string>
using std::string;

#include <sstream>
using std::stringstream;

#include <fstream>
using std::ofstream;

#define  INFO log.outfile << " INFO: " 
#define  WARN log.outfile << " WARN: " 
#define ERROR log.outfile << "ERROR: " 

class logger
{
public:

    ofstream outfile;

    logger()
    {
    }
    
    logger(const string& label)
    {
        stringstream ss;
        ss << getpid();
        string pid = ss.str();
        outfile.open((label + "_" + pid + ".txt").c_str(), std::ios::app);
    }
    
    ~logger()
    {
        outfile.close();
    }

};

#endif // logger_h

