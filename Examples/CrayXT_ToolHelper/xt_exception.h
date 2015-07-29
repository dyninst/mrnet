/****************************************************************************
 * Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller   *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef XT_TOOL_EXCEPTION_H
#define XT_TOOL_EXCEPTION_H

#include <stdexcept>

class XTMException : public std::runtime_error
{
public:
    XTMException( const char* m )
      : std::runtime_error( m )
    { }
};

#endif // XT_TOOL_EXCEPTION_H
