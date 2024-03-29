#include <cstdio>
#include <cfloat>

#include <hip/hip_runtime.h>
#include <omp.h>

#ifndef REPS
#define REPS 100000
#endif

// least common multiple of 104 and 110 times wavefront size
#define KERNEL_N (5720 * 64)

// Define macro to automate error handling of the HIP API calls
#define HIPCALL(func)                                                \
    {                                                                \
        hipError_t ret = func;                                       \
        if (ret != hipSuccess) {                                     \
            fprintf(stderr,                                          \
                    "HIP error: '%s' at %s:%d\n",                    \
                    hipGetErrorString(ret), __FUNCTION__, __LINE__); \
            abort();                                                 \
        }                                                            \
    }


__global__ void empty() {
    // do nothing!
}

int main(int argc, char const * argv[]) {
    int ncores;
    int ndev;
    double ** latency = NULL;
    double min_latency = DBL_MAX;
    const double usec = 1000.0 * 1000.0;

    // Determine number of cores and devices.
    HIPCALL(hipGetDeviceCount(&ndev));
    ncores = omp_get_num_procs();

    fprintf(stdout, "---------------------------------------------------------------\n");
    fprintf(stdout, "number of cores:   %d\n", ncores);
    fprintf(stdout, "number of devices: %d\n", ndev);
    fprintf(stdout, "number of repetitions: %d\n", REPS);
    fprintf(stdout, "---------------------------------------------------------------\n");

    // Streams to the devices
    hipStream_t * stream = new hipStream_t[ndev];
    
    // Allocate the memory to store the result data.
    latency = (double **)malloc(ncores * sizeof(double *));
    for (int c = 0; c < ncores; c++) {
        latency[c] = (double *)malloc(ndev * sizeof(double));
    }

    // Print the OpenMP thread affinity info.
    #pragma omp parallel num_threads(ncores)
    {
        omp_display_affinity(NULL);
    }
    fprintf(stdout, "---------------------------------------------------------------\n");

    // Perform some warm-up to make sure that all threads are up and running,
    // and the GPUs have been properly initialized.
    fprintf(stdout, "warm up...\n");
    #pragma omp parallel num_threads(ncores)
    {
        for (int c = 0; c < ncores; c++) {
            if (omp_get_thread_num() == c) {
                for (int d = 0; d < ndev; d++) {
                    HIPCALL(hipSetDevice(d));
                    HIPCALL(hipStreamCreate(&stream[d]));
                    empty<<<KERNEL_N, 64, 0, stream[d]>>>();
                    HIPCALL(hipStreamSynchronize(stream[d]));
                }
            }
            #pragma omp barrier
        }
    }
    fprintf(stdout, "---------------------------------------------------------------\n");

    // Perform the actual measurements.
    fprintf(stdout, "measurements...\n");
    double val = 0;
    #pragma omp parallel num_threads(ncores)
    {
        for (int c = 0; c < ncores; c++) {
            if (omp_get_thread_num() == c) {
                for (int d = 0; d < ndev; d++) {
                    fprintf(stdout, "running for thread=%3d and device=%2d\n", c, d);
                    fflush(stdout);
                    HIPCALL(hipSetDevice(d));
                    double ts = omp_get_wtime();
                    for (int r = 0; r < REPS; r++) {
                        empty<<<KERNEL_N, 64, 0, stream[d]>>>();
                    }
                    HIPCALL(hipStreamSynchronize(stream[d]));
                    double te = omp_get_wtime();
                    latency[c][d] = (te - ts) / ((double) REPS) * usec;
                    if(latency[c][d] < min_latency) {
                        min_latency = latency[c][d];
                    }
                }
            }
            #pragma omp barrier
        }
    }
    fprintf(stdout, "dummy=%f\n", val);
    fprintf(stdout, "---------------------------------------------------------------\n");


    fprintf(stdout, "---------------------------------------------------------------\n");
    fprintf(stdout, "Absolute measurements (us)\n");
    fprintf(stdout, "---------------------------------------------------------------\n");
    fprintf(stdout, ";");
    for (int c = 0; c < ncores; c++) {
        fprintf(stdout, "Core %d%c", c, c<ncores-1 ? ';' : '\n');
    }
    for (int d = 0; d < ndev; d++) {
        fprintf(stdout, "GPU %d;", d);
        for (int c = 0; c < ncores; c++) {
            fprintf(stdout, "%lf%c", latency[c][d], c<ncores-1 ? ';' : '\n');
        }
    }

    fprintf(stdout, "---------------------------------------------------------------\n");
    fprintf(stdout, "Relative measurements to minimum latency\n");
    fprintf(stdout, "---------------------------------------------------------------\n");
    fprintf(stdout, ";");
    for (int c = 0; c < ncores; c++) {
        fprintf(stdout, "Core %d%c", c, c<ncores-1 ? ';' : '\n');
    }
    for (int d = 0; d < ndev; d++) {
        fprintf(stdout, "GPU %d;", d);
        for (int c = 0; c < ncores; c++) {
            fprintf(stdout, "%lf%c", (latency[c][d] / min_latency), c<ncores-1 ? ';' : '\n');
        }
    }

    // cleanup
    delete[] stream;
    
    for (int c = 0; c < ncores; c++) {
        free(latency[c]);
    }
    free(latency);

    return 0;
}