//
// Created by xflord on 30.9.22.
//

#include <stdio.h>

#include "libcryptsetup.h"


int main() {
    struct crypt_device *cd;

    int r = crypt_init(&cd, "luksfile");
    printf("init = %i\n", r); 

    r = crypt_load(cd, CRYPT_LUKS1, NULL);
    printf("load = %i\n", r); 


    r = crypt_dump(cd);
    printf("dump = %i\n", r); 

}
