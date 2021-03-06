//
//  rs_const.h
//  Radar Simulator Framework
//
//  Created by Boon Leng Cheong on 2/10/16.
//  Copyright © 2016 Boon Leng Cheong. All rights reserved.
//

#ifndef rs_const_h
#define rs_const_h

#define RS_C                 29979458.0
#define RS_DOMAIN_PAD               2.0
#define RS_MAX_STR               4096
#define RS_MAX_GPU_PLATFORM        40
#define RS_MAX_GPU_DEVICE           8
#define RS_MAX_KERNEL_LINES      2048
#define RS_MAX_KERNEL_SRC      131072
#define RS_ALIGN_SIZE             128     // Align size. Be sure to have a least 16 for SSE, 32 for AVX, 64 for AVX-512
#define RS_MAX_GATES              512
#define RS_CL_GROUP_ITEMS          64
#define RS_MAX_DEBRIS_TYPES         8
#define RS_MAX_ADM_TABLES           RS_MAX_DEBRIS_TYPES
#define RS_MAX_RCS_TABLES           RS_MAX_DEBRIS_TYPES

#ifndef MAX
#define MAX(X, Y)      ((X) > (Y) ? (X) : (Y))
#endif
#ifndef MIN
#define MIN(X, Y)      ((X) > (Y) ? (Y) : (X))
#endif

#define DTIME(T_begin, T_end)  ((double)(T_end.tv_sec - T_begin.tv_sec) + 1.0e-6 * (double)(T_end.tv_usec - T_begin.tv_usec))
#define UNDERLINE(x)           "\033[4m" x "\033[24m"

enum RSStatus {
    RSStatusNull                         = 0,
    RSStatusPopulationDefined            = 1 << 1,
    RSStatusOriginsOffsetsDefined        = 1 << 2,
    RSStatusWorkersAllocated             = 1 << 3,
    RSStatusDomainPopulated              = 1 << 4,
    RSStatusScattererSignalNeedsUpdate   = 1 << 5,
    RSStatusDebrisRCSNeedsUpdate         = 1 << 6
};

enum RS_CL_PASS_2 {
    RS_CL_PASS_2_UNIVERSAL,
    RS_CL_PASS_2_IN_RANGE,
    RS_CL_PASS_2_IN_LOCAL
};

typedef char RSMethod;
enum RSMethod {
    RS_METHOD_CPU,
    RS_METHOD_GPU
};

#endif /* rs_const_h */
