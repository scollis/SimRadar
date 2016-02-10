//
//  radarsim.c
//  Radar Simulator
//
//  This is an example of how to use the Radar Simulator framework (librs.a).
//  All functions with prefix RS_ are built-in functions in the framework
//  and this example shows the general flow of using the framework to emulate.
//  a radar and generate time-series data. The RS framework is written in C
//  so any superset language, e.g., C++, Objective-C, and Visual C++, can use
//  the framework to generate radar data.
//
//  Framework requirements: OpenCL 1.1
//
//  Created by Boon Leng Cheong.
//
//

#include "rs.h"
#include "iq.h"

enum {
    ACCEL_TYPE_GPU,
    ACCEL_TYPE_CPU
};

//
//
//  M A I N
//
//
int main(int argc, char *argv[]) {
    
    char c;
    char verb = 0;
    char accel_type = 0;
    char output_file = FALSE;
    int num_frames = 5;
    float density = 0.0f;
    float scan_el = 3.0f;
    
    struct timeval t0, t1, t2;
    
    gettimeofday(&t0, NULL);
    
    int debris_types = 0;
    int debris_count[3] = {0, 0, 0};
    int warm_up_frames = 2000;
    
    while ((c = getopt(argc, argv, "vcgd:e:f:a:D:w:oh?")) != -1) {
        switch (c) {
            case 'v':
                verb++;
                break;
            case 'c':
                accel_type = ACCEL_TYPE_CPU;
                break;
            case 'g':
                accel_type = ACCEL_TYPE_GPU;
                break;
            case 'f':
                num_frames = atoi(optarg);
                break;
            case 'a':
                accel_type = atoi(optarg);
                break;
            case 'D':
                density = atof(optarg);
                break;
            case 'o':
                output_file = TRUE;
                break;
            case 'e':
                scan_el = atof(optarg);
                break;
            case 'd':
                debris_count[debris_types++] = atoi(optarg);
                break;
            case 'w':
                warm_up_frames = atoi(optarg);
                break;
            case 'h':
            case '?':
                printf("%s\n\n"
                       "%s [-v] [-c] [-g] FILE1 FILE2 ...\n\n"
                       "    -v    increases verbosity\n"
                       "    -c    CPU\n"
                       "    -g    GPU\n"
                       "    -f F  generates F frames\n"
                       "\n",
                       argv[0], argv[0]);
                return EXIT_FAILURE;
            default:
                fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
                break;
        }
    }
    
    printf("%s : Session started\n", now());
    
    RSHandle *S;
    ADMHandle *A;
    LESHandle *L;
    RCSHandle *R;
//    ARPSHandle *O;
    
    // Initialize the RS framework
    if (accel_type == ACCEL_TYPE_CPU) {
        S = RS_init_for_cpu_verbose(verb);
    } else {
        S = RS_init_verbose(verb);
    }
    if (S == NULL) {
        fprintf(stderr, "%s : Some errors occurred during RS_init().\n", now());
        return EXIT_FAILURE;
    }
    
    if (density > 0.0f) {
        RS_set_density(S, density);
    }
    
    // Initialize the LES ingest
    L = LES_init();
    if (L == NULL) {
        fprintf(stderr, "%s : Some errors occurred during LES_init().\n", now());
        return EXIT_FAILURE;
    }
    
    // Initialize the ADM ingest
    A = ADM_init();
    if (A == NULL) {
        fprintf(stderr, "%s : Some errors occurred during ADM_init().\n", now());
        return EXIT_FAILURE;
    }
    
    // Initialize the RCS ingest
    R = RCS_init();
    if (R == NULL) {
        fprintf(stderr, "%s : Some errors occurred during RCS_init().\n", now());
        return EXIT_FAILURE;
    }
    
    // Initialize the ARPS ingest
//    O = ARPS_init();
//    if (O == NULL) {
//        fprintf(stderr, "%s : Some errors occurred during ARPS_init().\n", now());
//        return EXIT_FAILURE;
//    }
    
    // Set up the parameters:
    // Only use the setter functions to change the state.
    RS_set_antenna_params(S, 1.0f, 44.5f);
    
    RS_set_tx_params(S, 0.2e-6, 50.0e3f);

    for (int k = 0; k < RS_MAX_VEL_TABLES; k++) {
        RS_set_vel_data_to_LES_table(S, LES_get_frame(L, k));
    }
    
    RS_set_adm_data_to_ADM_table(S, ADM_get_table(A, ADMConfigModelPlate));
    RS_set_adm_data_to_ADM_table(S, ADM_get_table(A, ADMConfigSquarePlate));
    
    RS_set_rcs_data_to_RCS_table(S, RCS_get_table(R, RCSConfigLeaf));
    RS_set_rcs_data_to_RCS_table(S, RCS_get_table(R, RCSConfigWoodBoard));

    RSBox box = RS_suggest_scan_domain(S, 16);
    
    // Set debris population
    for (int k = 0; k < debris_types; k++) {
        RS_set_debris_count(S, k + 1, debris_count[k]);
    }
    RS_revise_debris_counts_to_gpu_preference(S);
    
    // No need to go all the way up if we are looking low
    box.size.e = MIN(box.size.e, scan_el + RS_DOMAIN_PAD);
    
    RS_set_scan_box(S,
                    box.origin.r, box.origin.r + box.size.r, 15.0f,   // Range
                    box.origin.a, box.origin.a + box.size.a, 1.0f,    // Azimuth
                    box.origin.e, box.origin.e + box.size.e, 1.0f);   // Elevation
    
    //    RS_set_scan_box(S,
    //                    10.0e3, 14.0e3, 150.0f,                     // Range
    //                    -10.0f, 10.0f, 1.0f,                        // Azimuth
    //                    0.0f, 8.0f, 1.0f);                          // Elevation
    
    RS_set_dsd_to_mp(S);

    // Populate the domain with scatter bodies.
    // This is also the function that triggers kernel compilation, GPU memory allocation and
    // upload all the parameters to the GPU.
    RS_populate(S);
    
    // Show some basic info
    printf("%s : Emulating %s frame%s with %s scatter bodies\n",
           now(), commaint(num_frames), num_frames>1?"s":"", commaint(S->num_scats));

    // Now, we are ready to bake
    int k = 0;

    // Some warm up if we are going for real
    if (num_frames > 1200) {
        RS_set_prt(S, 1.0f / 60.0f);
        for (k = 0; k < warm_up_frames; k++) {
            if (k % 100 == 0) {
                fprintf(stderr, "Warming up ... \033[32m%.2f%%\033[0m  \r", (float)k / warm_up_frames * 100.0f);
            }
            RS_advance_time(S);
        }
    }

    // Set PRT to the actual one
    
    RS_set_prt(S, 1.0e-3f);

    // ---------------------------------------------------------------------------------------------------------------
    
    float az_deg = -12.0f, el_deg = scan_el;
    
    // Initialize a file if the user wants an output file
    FILE *fid = NULL;
    IQFileHeader file_header;
    IQPulseHeader pulse_header;
    memset(&file_header, 0, sizeof(IQFileHeader));
    memset(&pulse_header, 0, sizeof(IQPulseHeader));
    
    file_header.params = S->params;
    for (k = 0; k < S->num_debris; k++) {
        file_header.debris_population[k] = S->debris_population[k];
    }
    
    if (output_file) {
        char filename[4096];
        memset(filename, 0, 4096);
        snprintf(filename, 256, "%s/Downloads/sim-%s-E%04.1f.iq", getenv("HOME"), nowlong(), el_deg);
        printf("%s : Output file : \033[1;32m%s\033[0m\n", now(), filename);
        fid = fopen(filename, "wb");
        if (fid == NULL) {
            fprintf(stderr, "%s : Error creating file for writing data.\n", now());
            output_file = FALSE;
        }
        // For now, we simply write a 4K header. Will populate with more contents next time
        fwrite(&file_header, sizeof(IQFileHeader), 1, fid);
    }
    
    gettimeofday(&t1, NULL);
    
    float dt, fps, prog, eta;
    
    for (k = 0; k<num_frames; k++) {
        if (k % 100 == 0) {
            gettimeofday(&t2, NULL);
            dt = DTIME(t1, t2);
            t1 = t2;
            if (k > 100) {
                prog =  (float)k / num_frames * 100.0f;
                fps = 100.0f / dt;
                eta = (float)(num_frames - k) / fps;
                fprintf(stderr, "k = %d  az_deg = %.2f  el_deg = %.2f   %.2f fps  progress: \033[1;33m%.2f%%\033[0m   eta = %.0f second%s   \r", k, az_deg, el_deg, fps, prog, eta, eta > 1.5f ? "s" : "");
            } else {
                fprintf(stderr, "k = %d  az_deg = %.2f  el_deg = %.2f             \r", k, az_deg, el_deg);
            }
        }
        RS_set_beam_pos(S, az_deg, el_deg);
        RS_make_pulse(S);
        RS_advance_time(S);

        // Only download the necessary data
        if (verb > 2) {
            RS_download(S);
        } else if (output_file) {
            RS_download_pulse_only(S);
        }

//        if (verb > 2) {
//            RS_show_scat_sig(S);
//            
//            printf("signal:\n");
//            for (int r = 0; r < S->params.range_count; r++) {
//                printf("sig[%d] = (%.4f %.4f %.4f %.4f)\n", r, S->pulse[r].s0, S->pulse[r].s1, S->pulse[r].s2, S->pulse[r].s3);
//            }
//            printf("\n");
//        }
        
        if (output_file) {
            // Gather information for the  pulse header
            pulse_header.time = S->sim_time;
            pulse_header.az_deg = az_deg;
            pulse_header.el_deg = el_deg;
            
            fwrite(&pulse_header, sizeof(IQPulseHeader), 1, fid);
            fwrite(S->pulse, sizeof(cl_float4), S->params.range_count, fid);
        }

        // Update scan angles for the next pulse
        az_deg = fmodf(az_deg + 0.01f + 12.0f, 24.0f) - 12.0f;
    }
    
    // Clear the last line and beep five times
    fprintf(stderr, "%120s\r", "");
    #if defined (__APPLE__)
    system("say -v Bells dong dong dong dong &");
    #else
    fprintf(stderr, "\a\a\a\a\a");
    #endif
    
    gettimeofday(&t2, NULL);
    dt = DTIME(t0, t2);
    printf("%s : Finished.  Total time elapsed = %.2f s\n", now(), dt);
    
    if (verb > 2) {
        RS_download(S);
        printf("Final scatter body positions, velocities and orientations:\n");
        RS_show_scat_pos(S);
        RS_show_scat_sig(S);
    }

    if (output_file) {
        printf("%s : Data file with %s bytes.\n", now(), commaint(ftell(fid)));
        fclose(fid);
    }
    
    printf("%s : Session ended\n", now());

    RS_free(S);
    
    LES_free(L);
    
    ADM_free(A);
    
    RCS_free(R);
    
    //ARPS_free(O);
    
    return EXIT_SUCCESS;
}
