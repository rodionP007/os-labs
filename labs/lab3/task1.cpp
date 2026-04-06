#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef long long i64;

/* =========================
   SETTINGS
   ========================= */
#define STUDENT_TICKET 431418
#define N 100000000LL
/* ========================= */

typedef struct
{
    HANDLE threadHandle;

    HANDLE readyEvent;    // thread started
    HANDLE doneEvent;     // current block finished
    HANDLE parkedEvent;   // thread reached safe parking point
    HANDLE releaseEvent;  // allow thread to continue

    i64 begin;
    i64 end;
    i64 n;

    double partialSum;
    volatile LONG terminate;
} ThreadData;

static int assign_next_block(ThreadData* td, i64* nextBegin, i64 n, i64 blockSize)
{
    if (*nextBegin >= n)
        return 0;

    td->begin = *nextBegin;
    td->end = *nextBegin + blockSize;

    if (td->end > n)
        td->end = n;

    *nextBegin = td->end;
    return 1;
}

DWORD WINAPI worker_proc(LPVOID param)
{
    ThreadData* td = (ThreadData*)param;
    const double step = 1.0 / (double)td->n;

    // signal that thread is alive
    SetEvent(td->readyEvent);

    // wait for first assigned block
    WaitForSingleObject(td->releaseEvent, INFINITE);

    while (1)
    {
        if (td->terminate)
            break;

        double blockSum = 0.0;

        for (i64 i = td->begin; i < td->end; ++i)
        {
            double x = ((double)i + 0.5) * step;
            blockSum += 4.0 / (1.0 + x * x);
        }

        td->partialSum += blockSum * step;

        // block is done
        SetEvent(td->doneEvent);

        // reached safe point for suspension
        SetEvent(td->parkedEvent);

        // wait until controller gives next block
        WaitForSingleObject(td->releaseEvent, INFINITE);
    }

    return 0;
}

double run_experiment(int threadCount, int studentTicket, i64 n, double* outPi)
{
    const i64 blockSize = 10LL * studentTicket;

    ThreadData* threads = (ThreadData*)calloc(threadCount, sizeof(ThreadData));
    HANDLE* doneEvents = (HANDLE*)calloc(threadCount, sizeof(HANDLE));
    HANDLE* threadHandles = (HANDLE*)calloc(threadCount, sizeof(HANDLE));

    if (!threads || !doneEvents || !threadHandles)
    {
        printf("Memory allocation error.\n");
        free(threads);
        free(doneEvents);
        free(threadHandles);
        return -1.0;
    }

    for (int i = 0; i < threadCount; ++i)
    {
        threads[i].n = n;
        threads[i].partialSum = 0.0;
        threads[i].terminate = 0;

        threads[i].readyEvent   = CreateEvent(NULL, FALSE, FALSE, NULL);
        threads[i].doneEvent    = CreateEvent(NULL, FALSE, FALSE, NULL);
        threads[i].parkedEvent  = CreateEvent(NULL, FALSE, FALSE, NULL);
        threads[i].releaseEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        if (!threads[i].readyEvent || !threads[i].doneEvent ||
            !threads[i].parkedEvent || !threads[i].releaseEvent)
        {
            printf("CreateEvent failed for thread %d.\n", i);
            return -1.0;
        }

        threads[i].threadHandle = CreateThread(
            NULL,
            0,
            worker_proc,
            &threads[i],
            CREATE_SUSPENDED,
            NULL
        );

        if (!threads[i].threadHandle)
        {
            printf("CreateThread failed for thread %d.\n", i);
            return -1.0;
        }

        doneEvents[i] = threads[i].doneEvent;
        threadHandles[i] = threads[i].threadHandle;
    }

    // start all threads from suspended state
    for (int i = 0; i < threadCount; ++i)
        ResumeThread(threads[i].threadHandle);

    // wait until all threads are alive
    for (int i = 0; i < threadCount; ++i)
        WaitForSingleObject(threads[i].readyEvent, INFINITE);

    i64 nextBegin = 0;
    int activeCount = 0;
    double pi = 0.0;

    LARGE_INTEGER freq, t1, t2;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t1);

    // give first blocks
    for (int i = 0; i < threadCount; ++i)
    {
        if (assign_next_block(&threads[i], &nextBegin, n, blockSize))
        {
            SetEvent(threads[i].releaseEvent);
            activeCount++;
        }
    }

    while (activeCount > 0)
    {
        DWORD waitResult = WaitForMultipleObjects(threadCount, doneEvents, FALSE, INFINITE);

        if (waitResult < WAIT_OBJECT_0 || waitResult >= WAIT_OBJECT_0 + (DWORD)threadCount)
        {
            printf("WaitForMultipleObjects failed.\n");
            return -1.0;
        }

        int idx = (int)(waitResult - WAIT_OBJECT_0);
        activeCount--;

        // wait until thread reached safe parking point
        WaitForSingleObject(threads[idx].parkedEvent, INFINITE);

        // now suspend it explicitly as required by the task
        SuspendThread(threads[idx].threadHandle);

        if (assign_next_block(&threads[idx], &nextBegin, n, blockSize))
        {
            SetEvent(threads[idx].releaseEvent);
            ResumeThread(threads[idx].threadHandle);
            activeCount++;
        }
        else
        {
            threads[idx].terminate = 1;
            SetEvent(threads[idx].releaseEvent);
            ResumeThread(threads[idx].threadHandle);
        }
    }

    WaitForMultipleObjects(threadCount, threadHandles, TRUE, INFINITE);
    QueryPerformanceCounter(&t2);

    for (int i = 0; i < threadCount; ++i)
        pi += threads[i].partialSum;

    for (int i = 0; i < threadCount; ++i)
    {
        if (threads[i].threadHandle) CloseHandle(threads[i].threadHandle);
        if (threads[i].readyEvent)   CloseHandle(threads[i].readyEvent);
        if (threads[i].doneEvent)    CloseHandle(threads[i].doneEvent);
        if (threads[i].parkedEvent)  CloseHandle(threads[i].parkedEvent);
        if (threads[i].releaseEvent) CloseHandle(threads[i].releaseEvent);
    }

    free(threads);
    free(doneEvents);
    free(threadHandles);

    *outPi = pi;
    return (double)(t2.QuadPart - t1.QuadPart) / (double)freq.QuadPart;
}

int main(void)
{
    const int studentTicket = STUDENT_TICKET;
    const i64 n = N;
    const i64 blockSize = 10LL * studentTicket;

    int threadCounts[] = {1, 2, 4, 8, 12, 16};
    const int testCount = sizeof(threadCounts) / sizeof(threadCounts[0]);

    printf("========================================\n");
    printf("Lab 3. Task 1 (Win32 API)\n");
    printf("========================================\n");
    printf("N              = %lld\n", n);
    printf("Student ticket = %d\n", studentTicket);
    printf("Block size     = %lld\n\n", blockSize);

    printf("%-12s %-20s %-15s %-15s\n", "Threads", "pi", "Time(s)", "Speedup");
    printf("--------------------------------------------------------------\n");

    double baseTime = 0.0;

    for (int k = 0; k < testCount; ++k)
    {
        int threads = threadCounts[k];
        double pi = 0.0;

        printf("Starting %d threads...\n", threads);
        fflush(stdout);

        double elapsed = run_experiment(threads, studentTicket, n, &pi);
        if (elapsed < 0.0)
            return 1;

        if (k == 0)
            baseTime = elapsed;

        double speedup = baseTime / elapsed;

        printf("%-12d %-20.15f %-15.6f %-15.3f\n",
               threads, pi, elapsed, speedup);
    }

    return 0;
}
