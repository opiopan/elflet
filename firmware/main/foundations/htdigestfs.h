#pragma once

#include <stdint.h>
#include <stdio.h>

#define HTDIGESTLENGTH 16

#ifdef __cplusplus
extern "C" {
#endif
    
void htdigestfs_init(const char* mp);
void htdigestfs_register(const char* user, const char* domain, void* digest);
FILE* htdigestfs_fp();

#ifdef __cplusplus
}
#endif
