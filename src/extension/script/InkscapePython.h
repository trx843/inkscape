#ifndef __INKSCAPE_PYTHON_H__
#define __INKSCAPE_PYTHON_H__

/**
 * Python Interpreter wrapper for Inkscape
 *
 * Authors:
 *   Bob Jamison <rjamison@titan.com>
 *
 * Copyright (C) 2004 Inkscape.org
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include "InkscapeInterpreter.h"

namespace Inkscape
{
namespace Extension
{
namespace Script
{



class InkscapePython : public InkscapeInterpreter
{
public:

    /*
     *
     */
    InkscapePython();
    

    /*
     *
     */
    virtual ~InkscapePython();
    
    

    /*
     *
     */
    virtual bool interpretScript(char *str);
    
    

    /*
     *
     */
    virtual bool interpretFile(char *fileName);
    
private:


};


}  // namespace Script
}  // namespace Extension
}  // namespace Inkscape



#endif /*__INKSCAPE_PYTHON_H__ */
//#########################################################################
//# E N D    O F    F I L E
//#########################################################################



