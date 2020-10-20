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

#include <stdint.h>
#include <cstring>

namespace SEP
{

class Lutz;
class Deblend;
class Analyze;

class Extract
{
    public:

        Extract();
        ~Extract();

        int sep_extract(sep_image *image, float thresh, int thresh_type,
                        int minarea, float *conv, int convw, int convh,
                        int filter_type, int deblend_nthresh, double deblend_cont,
                        int clean_flag, double clean_param,
                        sep_catalog **catalog);

        static void free_catalog_fields(sep_catalog *catalog);
        static void sep_catalog_free(sep_catalog *catalog);

    protected:

        int sortit(infostruct *info, objliststruct *objlist, int minarea,
                   objliststruct *finalobjlist, int deblend_nthresh, double deblend_mincont, double gain);

        /* get and set pixstack */
        void sep_set_extract_pixstack(size_t val)
        {
            extract_pixstack = val;
        }

        size_t sep_get_extract_pixstack()
        {
            return extract_pixstack;
        }

        void plistinit(int hasconv, int hasvar);
        void clean(objliststruct *objlist, double clean_param, int *survives);


        int arraybuffer_init(arraybuffer *buf, void *arr, int dtype, int w, int h,
                             int bufw, int bufh);
        void arraybuffer_readline(arraybuffer *buf);
        void arraybuffer_free(arraybuffer *buf);

    private:

        std::unique_ptr<Deblend> deblend;
        std::unique_ptr<Lutz> lutz;
        std::unique_ptr<Analyze> analyze;

        int convert_to_catalog(objliststruct *objlist, int *survives, sep_catalog *cat, int w, int include_pixels);
        void apply_mask_line(arraybuffer *mbuf, arraybuffer *imbuf, arraybuffer *nbuf);

        int plistexist_cdvalue, plistexist_thresh, plistexist_var;
        int plistoff_value, plistoff_cdvalue, plistoff_thresh, plistoff_var;
        int plistsize;
        size_t extract_pixstack = 300000;
        plistvalues plist_values;
        objstruct obj;
};

}
