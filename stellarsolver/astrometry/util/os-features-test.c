/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */
#include <stdlib.h>
#include <stdio.h>

#ifdef TEST_CANONICALIZE_FILE_NAME
int main() {
    char* path = canonicalize_file_name("/");
    free(path);
    //printf("#define NEED_CANONICALIZE_FILE_NAME 0\n");
    return 0;
}
#endif

#if defined(TEST_NETPBM) || defined(TEST_NETPBM_MAKE)
#include <pam.h>
int main(int argc, char** args) {
    struct pam img;
    pm_init(args[0], 0);
    //printf("#define HAVE_NETPBM 1\n");
    img.size = 42;
    printf("the answer is %i\n", img.size);
    return 0;
}
#endif
