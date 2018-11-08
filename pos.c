//
//  pos.c
//  Radar Simulation Framework
//
//  Created by Boonleng Cheong 9/12/2018
//  Copyright (c) 2018 Boonleng Cheong. All rights reserved.
//

#include "pos.h"

int POS_get_next_angles(POSPattern *scan) {
    int k = scan->index;
    // Current azimuth and elevation
    scan->az = scan->positions[k].az;
    scan->el = scan->positions[k].el;
    #if defined(DEBUG_POS)
    rsprint("%d / %d   %d / %d  %.2f %.2f\n",
            scan->index, scan->count,
            scan->positions[k].index, scan->positions[k].count,
            scan->az, scan->el);
    #endif
    // Update internal indices for next iteration
    scan->positions[k].index++;
    if (scan->positions[k].index == scan->positions[k].count) {
        scan->positions[k].index = 0;
        scan->index++;
        if (scan->index == scan->count) {
            scan->index = 0;
        }
    }
    return 0;
}

int POS_parse_from_string(POSPattern *scan, const char *string) {
    int j, k;
    float f1, f2, f3;
    const char delim[] = "/";
    char scan_pattern[1024], *token;
    rsprint("Parsing scanning pattern '%s' ...\n", string);
    scan->mode = string[0];
    switch (scan->mode) {
        case 'p':
        case 'P':
            // PPI
            rsprint("PPI scanning pattern ... (coming soon)\n");
            break;
        case 'r':
        case 'R':
            // RHI
            rsprint("RHI scanning pattern ... (coming soon)\n");
            break;
        case 'd':
        case 'D':
            rsprint("DBS scanning pattern ...\n");
            if (string[1] != ':') {
                rsprint("Expected : after the scan mode character.\n");
                return 1;
            }
            strcpy(scan_pattern, string + 2);
            token = strtok(scan_pattern, delim);
            j = 0;
            while (token && j < POS_MAX_PATTERN_COUNT) {
                k = sscanf(token, "%f,%f,%f", &f1, &f2, &f3);
                #if defined(DEBUG_POS)
                printf("k = %d   f1 = %.3f   f2 = %.3f   f3 = %.3f\n", k, f1, f2, f3);
                #endif
                scan->positions[j].az = f1;
                scan->positions[j].el = f2;
                scan->positions[j].count = (uint32_t)f3;
                rsprint("POS: j = %d   el = %5.2f   az = %6.2f   count = %u \n",
                        j, scan->positions[j].el, scan->positions[j].az, scan->positions[j].count);
                token = strtok(NULL, delim);
                j++;
            }
            scan->count = j;
            break;
        default:
            break;
    }
    
    return 0;
}

bool POS_is_ppi(POSPattern *scan) {
    return scan->mode == 'p' || scan->mode == 'P';
}

bool POS_is_rhi(POSPattern *scan) {
    return scan->mode == 'r' || scan->mode == 'R';
}

bool POS_is_dbs(POSPattern *scan) {
    return scan->mode == 'd' || scan->mode == 'D';
}