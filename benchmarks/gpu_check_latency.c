#include <stdio.h>
#include <omp.h>

#ifndef REPS
#define REPS 1000
#endif

int main(int argc, char const * argv[]) {
    int ncores;
    int ndev;
    double ** latency = NULL;
    const double usec = 1000.0 * 1000.0;

    // Determine number of cores and devices.
    ndev = omp_get_num_devices();
    ncores = omp_get_num_procs();

    fprintf(stdout, "---------------------------------------------------------------\n");
    fprintf(stdout, "number of cores:   %d\n", ncores);
    fprintf(stdout, "number of devices: %d\n", ndev);
    fprintf(stdout, "---------------------------------------------------------------\n");

    // Allocate the memory to store the result data.
    latency = (double **)malloc(ncores * sizeof(double *));
    for (int i = 0; i < ncores; i++) {
        latency[i] = (double *)malloc(ncores * sizeof(double));
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
                    #pragma omp target device(d)
                    {
                        // do nothing
                    }
                    if (!d) {
                        fprintf(stdout, "#");
                        fflush(stdout);
                    }
                    else {
                        fprintf(stdout, ".");
                        fflush(stdout);
                    }
                }
            }
            #pragma omp barrier
        }
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "---------------------------------------------------------------\n");

    // Perform the actual measurements.
    fprintf(stdout, "measurements...\n");
    double val = 0;
    #pragma omp parallel num_threads(ncores)
    {
        for (int c = 0; c < ncores; c++) {
            if (omp_get_thread_num() == c) {
                for (int d = 0; d < ndev; d++) {
                // for (int d = ndev-1; d >= 0; d--) {
                    double ts = omp_get_wtime();
                    for (int r = 0; r < REPS; r++) {
                        #pragma omp target device(d) map(tofrom:val)
                        {
                            // do almost nothing
                            val += c*d+r;
                        }
                    }
                    double te = omp_get_wtime();
                    latency[c][d] = (te - ts) / ((double) REPS) * usec;
                    if (!d) {
                        fprintf(stdout, "#");
                        fflush(stdout);
                    }
                    else {
                        fprintf(stdout, ".");
                        fflush(stdout);
                    }
                }
            }
            #pragma omp barrier
        }
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "dummy=%f\n", val);
    fprintf(stdout, "---------------------------------------------------------------\n");

    // Output the result data as CSV to the console.
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

    return 0;
}