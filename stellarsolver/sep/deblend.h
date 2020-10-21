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

#pragma once

#include "sep.h"
#include "sepcore.h"


namespace SEP
{

class Lutz;
class Analyze;

class Deblend
{
    public:
        Deblend(int deblend_nthresh, const plistvalues &values);
        ~Deblend();

        int deblend(objliststruct *objlistin, int l, objliststruct *objlistout,
                    int deblend_nthresh, double deblend_mincont, int minarea, SEP::Lutz *lutz);

    protected:

        int belong(int, objliststruct *, int, objliststruct *);
        int *createsubmap(objliststruct *, int, int *, int *, int *, int *);
        int gatherup(objliststruct *, objliststruct *);


    private:

        int allocdeblend(int deblend_nthresh);
        void freedeblend(void);

        objliststruct *objlist = nullptr;
        short *son = nullptr, *ok = nullptr;

        objliststruct	debobjlist, debobjlist2;
        plistvalues plist_values;
        int plistsize;
};

}

