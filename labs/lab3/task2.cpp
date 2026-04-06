#include <iostream>
#include <iomanip>
#include <omp.h>

using namespace std;

typedef long long i64;

/* =========================
   SETTINGS
   ========================= */
const int STUDENT_TICKET = 431418;      // change if needed
const i64 N = 100000000LL;
/* ========================= */

double compute_pi_openmp(int threadCount, int studentTicket, i64 n)
{
    const int blockSize = 10 * studentTicket;
    const double step = 1.0 / (double)n;
    double sum = 0.0;

    omp_set_num_threads(threadCount);

    #pragma omp parallel for schedule(dynamic, blockSize) reduction(+:sum)
    for (i64 i = 0; i < n; ++i)
    {
        double x = ((double)i + 0.5) * step;
        sum += 4.0 / (1.0 + x * x);
    }

    return sum * step;
}

int main()
{
    const int studentTicket = STUDENT_TICKET;
    const i64 n = N;
    const int blockSize = 10 * studentTicket;

    int threadCounts[] = {1, 2, 4, 8, 12, 16};
    const int testCount = sizeof(threadCounts) / sizeof(threadCounts[0]);

    cout << "========================================\n";
    cout << "Lab 3. Task 2 (OpenMP)\n";
    cout << "========================================\n";
    cout << "N              = " << n << "\n";
    cout << "Student ticket = " << studentTicket << "\n";
    cout << "Block size     = " << blockSize << "\n\n";

    cout << left
         << setw(12) << "Threads"
         << setw(20) << "pi"
         << setw(15) << "Time(s)"
         << setw(15) << "Speedup"
         << "\n";

    cout << "--------------------------------------------------------------\n";

    double baseTime = 0.0;

    for (int k = 0; k < testCount; ++k)
    {
        int threads = threadCounts[k];

        double t1 = omp_get_wtime();
        double pi = compute_pi_openmp(threads, studentTicket, n);
        double t2 = omp_get_wtime();

        double elapsed = t2 - t1;

        if (k == 0)
            baseTime = elapsed;

        double speedup = baseTime / elapsed;

        cout << left
             << setw(12) << threads
             << setw(20) << fixed << setprecision(15) << pi
             << setw(15) << fixed << setprecision(6) << elapsed
             << setw(15) << fixed << setprecision(3) << speedup
             << "\n";
    }

    return 0;
}
