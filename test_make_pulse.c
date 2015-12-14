/*
 *
 * Test a reduction kernel
 *
 */

#include <errno.h>
#include "rs.h"
#include "rs_priv.h"

#define NUM_ELEM      (1079196)
//#define NUM_ELEM      (102400)
//#define NUM_ELEM      (64)
#define RANGE_GATES   (9)
#define GROUP_ITEMS   (64)
#define GROUP_COUNTS  (64)

enum {
	TEST_N0NE        = 0,
	TEST_CPU         = 1,
	TEST_GPU_PASS_1  = 1 << 1,
	TEST_GPU_PASS_2  = 1 << 2,
	TEST_GPU         = TEST_GPU_PASS_1 | TEST_GPU_PASS_2,
	TEST_ALL         = TEST_CPU | TEST_GPU
};



int main(int argc, char **argv)
{
	char c;
	char verb = 0;
	char test = TEST_N0NE;
	const cl_float4 zero = {{0.0f, 0.0f, 0.0f, 0.0f}};

	unsigned int speed_test_iterations = 0;
	
	struct timeval t1, t2;
	
	cl_float4 *host_sig;
	cl_float4 *host_att;
	cl_float4 *cpu_pulse;
	
    cl_uint num_devices;
    cl_device_id devices[4];
    cl_uint vendors[4];
    cl_uint num_cus[4];
    
	int err;
	cl_int ret;
	cl_context context;
	cl_program program;
	cl_mem sig;
	cl_mem att;
	cl_mem work;
	cl_mem pulse;
	cl_mem range_weight;
	cl_kernel kernel_pop;
	cl_kernel kernel_make_pulse_pass_1;
	cl_kernel kernel_make_pulse_pass_2;
	cl_command_queue queue;
	
	size_t size = 0;
	size_t max_workgroup_size = 0;
	
	unsigned int num_elem = NUM_ELEM;

	while ((c = getopt(argc, argv, "ac12gvn:p:h?")) != -1) {
		switch (c) {
			case 'a':
				test = TEST_ALL;
				break;
			case '1':
				test |= TEST_GPU_PASS_1;
				break;
			case '2':
				test |= TEST_GPU_PASS_2;
				break;
			case 'g':
				test |= TEST_GPU;
				break;
			case 'c':
				test |= TEST_CPU;
				break;
			case 'v':
				verb++;
				break;
			case 'n':
				speed_test_iterations = atoi(optarg);
				break;
			case'p':
				num_elem = atoi(optarg);
				break;
			case 'h':
			case '?':
				printf("%s\n\n"
					   "%s [OPTIONS]\n\n"
					   "    -a     All CPU & GPU tests\n"
					   "    -c     CPU test\n"
					   "    -1     GPU Pass 1 test\n"
					   "    -2     GPU Pass 2 test\n"
					   "    -g     All GPU Tests\n"
					   "    -v     increases verbosity\n"
					   "    -n N   speed test using N iterations\n"
					   "\n",
					   argv[0], argv[0]);
				return EXIT_FAILURE;
			default:
				fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				break;
		}
	}
	
	if (test & TEST_ALL && speed_test_iterations == 0) {
		speed_test_iterations = 100;
	}
	
	// Check for some info
    get_device_info(CL_DEVICE_TYPE_GPU, &num_devices, devices, num_cus, vendors, verb);
    
	// Get the OpenCL devices
	ret = clGetDeviceInfo(devices[0], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &max_workgroup_size, &size);
	if (ret != CL_SUCCESS) {
		fprintf(stderr, "%s : Unable to obtain CL_DEVICE_MAX_WORK_GROUP_SIZE.\n", now());
		exit(EXIT_FAILURE);
	}
	
	// OpenCL context. Use the 1st device
	context = clCreateContext(NULL, 1, devices, &pfn_notify, NULL, &ret);
	if (ret != CL_SUCCESS) {
		fprintf(stderr, "%s : Error creating OpenCL context.  ret = %d\n", now(), ret);
		exit(EXIT_FAILURE);
	}
	
	char *src_ptr[RS_MAX_KERNEL_LINES];
	cl_uint len = read_kernel_source_from_files(src_ptr, "rs.cl", NULL);
	
	// Program
	program = clCreateProgramWithSource(context, len, (const char **)src_ptr, NULL, &ret);
	if (clBuildProgram(program, 1, devices, "", NULL, NULL) != CL_SUCCESS) {
		char char_buf[RS_MAX_STR];
		clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, RS_MAX_STR, char_buf, NULL);
		fprintf(stderr, "CL Compilation failed:\n%s", char_buf);
		exit(EXIT_FAILURE);
	}
	
	cl_ulong buf_ulong;
	clGetDeviceInfo(devices[0], CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(cl_ulong), &buf_ulong, NULL);
	if (RANGE_GATES * GROUP_ITEMS * sizeof(cl_float4) > buf_ulong) {
		fprintf(stderr, "Local memory size exceeded.  %d > %d\n",
				(int)(RANGE_GATES * GROUP_ITEMS * sizeof(cl_float4)),
				(int)buf_ulong);
		exit(EXIT_FAILURE);
	}

	// Command queue
    queue = clCreateCommandQueue(context, devices[0], 0, &ret);
    if (ret != CL_SUCCESS) {
        fprintf(stderr, "Error creating queue.\n");
        exit(EXIT_FAILURE);
    }
	
	// CPU memory
	host_sig = (cl_float4 *)malloc(MAX(num_elem, RANGE_GATES * GROUP_ITEMS) * sizeof(cl_float4));
	host_att = (cl_float4 *)malloc(MAX(num_elem, RANGE_GATES * GROUP_ITEMS) * sizeof(cl_float4));
	cpu_pulse = (cl_float4 *)malloc(RANGE_GATES * sizeof(cl_float4));

	float table_range_start = -25.0f;
	float table_range_delta = 25.0f;
	float range_weight_cpu[3] = {0.0f, 1.0f, 0.0f};
	
	// GPU memory
	sig = clCreateBuffer(context, CL_MEM_READ_WRITE, num_elem * sizeof(cl_float4), NULL, &ret);
	att = clCreateBuffer(context, CL_MEM_READ_WRITE, num_elem * sizeof(cl_float4), NULL, &ret);
	work = clCreateBuffer(context, CL_MEM_READ_WRITE, RANGE_GATES * GROUP_ITEMS * sizeof(cl_float4), NULL, &ret);
	pulse = clCreateBuffer(context, CL_MEM_READ_WRITE, RANGE_GATES * sizeof(cl_float4), NULL, &ret);
	range_weight = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, 3 * sizeof(cl_float), range_weight_cpu, &ret);

	// Populate the input data
	printf("Number of points = %d\n", num_elem);
	
	kernel_pop = clCreateKernel(program, "pop", &ret);
	if (ret != CL_SUCCESS) {
		fprintf(stderr, "Error\n");
		exit(EXIT_FAILURE);
	}
	clSetKernelArg(kernel_pop, 0, sizeof(cl_mem), &sig);
	clSetKernelArg(kernel_pop, 1, sizeof(cl_mem), &att);
	
	size = num_elem;
	clEnqueueNDRangeKernel(queue, kernel_pop, 1, NULL, &size, NULL, 0, NULL, NULL);
	clEnqueueReadBuffer(queue, sig, CL_TRUE, 0, num_elem * sizeof(cl_float4), host_sig, 0, NULL, NULL);
	clEnqueueReadBuffer(queue, att, CL_TRUE, 0, num_elem * sizeof(cl_float4), host_att, 0, NULL, NULL);

    err = clFinish(queue);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error: Failed in clFinish().\n");
        exit(EXIT_FAILURE);
    }

    // Table parameters
	const float range_weight_table_dx = 1.0f / table_range_delta;
	const float range_weight_table_x0 = -table_range_start * range_weight_table_dx;
	const float range_weight_table_xm = 2.0;  // Table only has 3 entries
	printf("Table set.  dx = %.2f  x0 = %.2f  xm = %.0f\n", range_weight_table_dx, range_weight_table_x0, range_weight_table_xm);

	// Global / local parameterization for CL kernels
	RSMakePulseParams R = RS_make_pulse_params(num_elem, GROUP_ITEMS, GROUP_COUNTS, 1.0f, 0.25f, RANGE_GATES);
	
	//
	// CPU calculation
	//

	for (int ir=0; ir<RANGE_GATES; ir++) {
		cpu_pulse[ir] = zero;
		float r = (float)ir * R.range_delta + R.range_start;

		for (int i=0; i<num_elem; i++) {
			float r_a = host_att[i].s0;
			float w_r = read_table(range_weight_cpu, range_weight_table_xm, (r_a - r) * range_weight_table_dx + range_weight_table_x0);
//			if (ir < 2) {
//				float fidx = (r_a - r) * range_weight_table_dx + range_weight_table_x0;
//				printf("ir=%2u  r=%5.2f  i=%2u  r_a=%.3f  dr=%.3f  w_r=%.3f  %.2f -> %.0f/%.0f/%.2f\n",
//					   ir, r, i, r_a, r_a-r, w_r, fidx, floorf(fidx), ceilf(fidx), fidx-floorf(fidx));
//			}
			cpu_pulse[ir].s0 += host_sig[i].s0 * w_r;
			cpu_pulse[ir].s1 += host_sig[i].s1 * w_r;
			cpu_pulse[ir].s2 += host_sig[i].s2 * w_r;
			cpu_pulse[ir].s3 += host_sig[i].s3 * w_r;
		}
	}
	
	if (verb > 3) {
		int i;
		cl_float4 v;
		printf("Input:\n");
		for (i=0; i<MIN(32, num_elem); i++) {
			clEnqueueReadBuffer(queue, sig, CL_TRUE, i * sizeof(cl_float4), sizeof(cl_float4), &v, 0, NULL, NULL);
			printf("%7d :  %9.1f  %9.1f  %9.1f  %9.1f\n", i, v.x, v.y, v.z, v.w);
		}
		for (i=MAX(i, num_elem-3); i<num_elem; i++) {
			clEnqueueReadBuffer(queue, sig, CL_TRUE, i * sizeof(cl_float4), sizeof(cl_float4), &v, 0, NULL, NULL);
			printf("%7d :  %9.1f  %9.1f  %9.1f  %9.1f\n", i, v.x, v.y, v.z, v.w);
		}
		printf("\n");
	}

	// Pass 1
	printf("Pass 1   global=%5d   local=%3d   groups=%3d   entries=%7d  local_mem=%5zu (%2d x %d cl_float4)\n",
		   (int)R.global[0],
		   (int)R.local[0],
		   R.group_counts[0],
		   R.entry_counts[0],
		   R.local_mem_size[0],
		   R.range_count,
		   (int)R.local[0]);

	kernel_make_pulse_pass_1 = clCreateKernel(program, "make_pulse_pass_1", &ret);
	if (ret != CL_SUCCESS) {
		fprintf(stderr, "Error: Failed to compile kernel.\n");
		exit(EXIT_FAILURE);
	}

	
	err = CL_SUCCESS;
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 0, sizeof(cl_mem), &work);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 1, sizeof(cl_mem), &sig);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 2, sizeof(cl_mem), &att);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 3, R.local_mem_size[0], NULL);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 4, sizeof(cl_mem), &range_weight);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 5, sizeof(float), &range_weight_table_x0);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 6, sizeof(float), &range_weight_table_xm);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 7, sizeof(float), &range_weight_table_dx);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 8, sizeof(float), &R.range_start);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 9, sizeof(float), &R.range_delta);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 10, sizeof(unsigned int), &R.range_count);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 11, sizeof(unsigned int), &R.group_counts[0]);
	err |= clSetKernelArg(kernel_make_pulse_pass_1, 12, sizeof(unsigned int), &R.entry_counts[0]);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Error: Failed to set kernel arguments.\n");
		exit(EXIT_FAILURE);
	}
	
	// Should check against hardware limits
//	ret = clGetKernelWorkGroupInfo(kernel_make_pulse_pass_1, devices[0], CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &work_group_size, NULL);
//	printf("%s : CL_KERNEL_WORK_GROUP_SIZE = %zu\n", now(), work_group_size);

	

	// Pass 2
	printf("Pass 2   global=%5d   local=%3d   groups=%3d   entries=%7d  local_mem=%5zu ( 1 x %2lu cl_float4)  [%s]\n",
		   (int)R.global[1],
		   (int)R.local[1],
		   R.group_counts[1],
		   R.entry_counts[1],
		   R.local_mem_size[1], R.local_mem_size[1] / sizeof(cl_float4),
		   R.cl_pass_2_method == RS_CL_PASS_2_IN_RANGE ? "Range" :
		   (R.cl_pass_2_method == RS_CL_PASS_2_IN_LOCAL ? "Local" : "Universal"));

	if (R.cl_pass_2_method == RS_CL_PASS_2_IN_RANGE) {
		kernel_make_pulse_pass_2 = clCreateKernel(program, "make_pulse_pass_2_range", &ret);
	} else if (R.cl_pass_2_method == RS_CL_PASS_2_IN_LOCAL) {
		kernel_make_pulse_pass_2 = clCreateKernel(program, "make_pulse_pass_2_local", &ret);
	} else {
		kernel_make_pulse_pass_2 = clCreateKernel(program, "make_pulse_pass_2_group", &ret);
	}
	if (ret != CL_SUCCESS) {
		fprintf(stderr, "Error: Failed to compile kernel.\n");
		exit(EXIT_FAILURE);
	}
	err = CL_SUCCESS;
	err |= clSetKernelArg(kernel_make_pulse_pass_2, 0, sizeof(cl_mem), &pulse);
	err |= clSetKernelArg(kernel_make_pulse_pass_2, 1, sizeof(cl_mem), &work);
	err |= clSetKernelArg(kernel_make_pulse_pass_2, 2, R.local_mem_size[1], NULL);
	err |= clSetKernelArg(kernel_make_pulse_pass_2, 3, sizeof(unsigned int), &R.range_count);
	err |= clSetKernelArg(kernel_make_pulse_pass_2, 4, sizeof(unsigned int), &R.entry_counts[1]);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Error: Failed to set kernel arguments.\n");
		exit(EXIT_FAILURE);
	}

    cl_event events[2];
    
	err = CL_SUCCESS;
	err |= clEnqueueNDRangeKernel(queue, kernel_make_pulse_pass_1, 1, NULL, &R.global[0], &R.local[0], 0, NULL, &events[0]);
	err |= clEnqueueNDRangeKernel(queue, kernel_make_pulse_pass_2, 1, NULL, &R.global[1], &R.local[1], 1, &events[0], &events[1]);
    err |= clEnqueueReadBuffer(queue, pulse, CL_TRUE, 0, R.range_count * sizeof(cl_float4), host_sig, 1, &events[1], NULL);
	if (err != CL_SUCCESS) {
		fprintf(stderr, "Error: Failed in clEnqueueNDRangeKernel() and/or clEnqueueReadBuffer().\n");
		exit(EXIT_FAILURE);
	}

	err = clFinish(queue);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "Error: Failed in clFinish().\n");
        exit(EXIT_FAILURE);
    }

	printf("CPU Pulse :");
	for (int j=0; j<RANGE_GATES; j++) {
		printf(" %.1f", cpu_pulse[j].s0);
	}
	printf("\n");

	printf("GPU Pulse :");
	for (int j=0; j<R.range_count; j++) {
		printf(" %.1f", host_sig[j].s0);
	}
	printf("\n");

	printf("Deltas    :");
	float delta = 0.0f, avg_delta = 0.0f;
	for (int j=0; j<R.range_count; j++) {
		delta = (cpu_pulse[j].s0 - host_sig[j].s0) / MAX(1.0f, host_sig[j].s0);
		avg_delta += delta;
		printf(" %.1e", delta);
	}
	avg_delta /= R.range_count;
	printf("\n");
	printf("Delta avg : %e\n", avg_delta);
	
	if (test & TEST_ALL) {
		int k = 0;
		double t = 0.0f;

		printf("Running speed tests:\n");
		
		if (test & TEST_CPU) {
			gettimeofday(&t1, NULL);
			for (k=0; k<speed_test_iterations; k++) {
				// Make a pulse
				for (int ir=0; ir<RANGE_GATES; ir++) {
					cpu_pulse[ir] = zero;
					float r = (float)ir * R.range_delta + R.range_start;
					
					for (int k=0; k<num_elem; k++) {
						float r_a = host_att[k].s0;
						float w_r = read_table(range_weight_cpu, range_weight_table_xm, (r_a - r) * range_weight_table_dx + range_weight_table_x0);

						cpu_pulse[ir].s0 += host_sig[k].s0 * w_r;
						cpu_pulse[ir].s1 += host_sig[k].s1 * w_r;
						cpu_pulse[ir].s2 += host_sig[k].s2 * w_r;
						cpu_pulse[ir].s3 += host_sig[k].s3 * w_r;
					}
				}
			}
			gettimeofday(&t2, NULL);
			t = DTIME(t1, t2);
			printf("CPU Exec Time = %6.2f ms\n",
				   t / speed_test_iterations * 1000.0f);
		}

		if (test & TEST_GPU_PASS_1) {
			gettimeofday(&t1, NULL);
			for (k=0; k<speed_test_iterations; k++) {
				clEnqueueNDRangeKernel(queue, kernel_make_pulse_pass_1, 1, NULL, &R.global[0], &R.local[0], 1, NULL, NULL);
			}
			clFinish(queue);
			gettimeofday(&t2, NULL);
			t = DTIME(t1, t2);
			printf("GPU Exec Time = %6.2f ms   Throughput = %5.2f GB/s  (Pass 1)\n",
				   t / speed_test_iterations * 1000.0f,
				   1e-9 * R.entry_counts[0] * 2 * sizeof(cl_float4) * speed_test_iterations / t);
		}

		if (test & TEST_GPU_PASS_2) {
			gettimeofday(&t1, NULL);
			for (k=0; k<speed_test_iterations; k++) {
				clEnqueueNDRangeKernel(queue, kernel_make_pulse_pass_2, 1, NULL, &R.global[1], &R.local[1], 0, NULL, NULL);
			}
			clFinish(queue);
			gettimeofday(&t2, NULL);
			t = DTIME(t1, t2);
			printf("GPU Exec Time = %6.2f ms   Throughput = %5.2f GB/s  (Pass 2)\n",
				   t / speed_test_iterations * 1000.0f,
				   1e-9 * R.entry_counts[1] * sizeof(cl_float4) * speed_test_iterations / t);
		}
	}

	free(cpu_pulse);

	free(host_sig);
	free(host_att);
	
    clReleaseCommandQueue(queue);

	clReleaseKernel(kernel_pop);
	clReleaseKernel(kernel_make_pulse_pass_1);
	clReleaseKernel(kernel_make_pulse_pass_2);
	clReleaseMemObject(sig);
	clReleaseMemObject(att);
	clReleaseMemObject(work);
	clReleaseMemObject(pulse);
	clReleaseMemObject(range_weight);
	clReleaseProgram(program);
	clReleaseContext(context);
	
	return 0;
}
