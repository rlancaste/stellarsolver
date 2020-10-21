/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* This file is part of SEP
*
* Copyright 1993-2011 Emmanuel Bertin -- IAP/CNRS/UPMC
* Copyright 2014 SEP developers
*
* SEP is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SEP is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with SEP.  If not, see <http://www.gnu.org/licenses/>.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "sep.h"
#include "sepcore.h"

namespace SEP
{

class Analyze
{
public:
    Analyze(const plistvalues &values);

    void  analyse(int no, objliststruct *objlist, int robust, double gain);
    int analysemthresh(int objnb, objliststruct *objlist, int minarea, PIXTYPE thresh);
    void  preanalyse(int no, objliststruct *objlist);
protected:

private:

    int plistexist_cdvalue, plistexist_thresh, plistexist_var;
    int plistoff_value, plistoff_cdvalue, plistoff_thresh, plistoff_var;
    //int plistsize;

};

}
