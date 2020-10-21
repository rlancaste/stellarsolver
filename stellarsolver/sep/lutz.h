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

#include "extract.h"

#include <stdint.h>

namespace SEP
{
class Analyze;
class Lutz
{
    public:
        explicit Lutz(int width, int height, Analyze *a, const plistvalues &values);
        ~Lutz();

        void lutzsort(infostruct *, objliststruct *);

        int lutz(pliststruct *plistin,
                 int *objrootsubmap, int subx, int suby, int subw,
                 objstruct *objparent, objliststruct *objlist, int minarea);

        void  update(infostruct *infoptr1, infostruct *infoptr2, pliststruct *pixel);


    protected:
        int lutzalloc(int width, int height);
        void lutzfree();

    private:

        infostruct  *info = nullptr, *store = nullptr;
        char	   *marker = nullptr;
        pixstatus   *psstack = nullptr;
        int         *start = nullptr, *end = nullptr, *discan = nullptr;
        int         xmin, ymin, xmax, ymax;
        infostruct	curpixinfo, initinfo;

        plistvalues plist_values;
        int plistoff_cdvalue;
        int plistsize;

        Analyze *analyzer {nullptr};

        static const uint32_t NOBJ {256};
};

}
