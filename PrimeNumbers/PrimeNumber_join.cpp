
#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <intrin.h>
#include <conio.h>
#include <queue>
#include <thread>
#include <chrono>
#include "ProcessorInfo.h"
#include "common.h"
#include "t_join.h"


t_join* joinData = nullptr;
std::chrono::steady_clock::time_point beginTimer;
unsigned __int64 start;

class ThreadInput
{
public:
    // Just to keep track of answers so the compiler doesn't discard them
    // during optimization.
    int answer;
    int processed;

    // Input to the Threadworker
    int threadId;
    ulong* input;
    int count;

    // Output from the processing
    ulong totalIterations;
    int hardWaitCount;
    int softWaitCount;

    unsigned __int64 softWaitTimeTicks;
    unsigned __int64 softWaitWakeupTimeTicks;
    unsigned __int64 hardWaitWakeupTimeTicks;

    ThreadInput(int threadId, int numPrimeNumbers) :
        threadId(threadId),
        count(numPrimeNumbers),
        answer(0),
        processed(0),
        input(nullptr),
        totalIterations(0),
        hardWaitCount(0),
        softWaitCount(0) {}
};

const double _1Q = pow(10, 15);
const double _1T = pow(10, 12);
const double _1B = pow(10, 9);
const double _1M = pow(10, 6);
const double _1K = pow(10, 3);
const char* formatNumber(long long input)
{
    double scaledInput = input;
    char scaledUnit = ' ';
    if (input > _1Q)
    {
        scaledInput /= _1Q;
        scaledUnit = 'Q';
    }
    else if (input > _1T)
    {
        scaledInput /= _1T;
        scaledUnit = 'T';
    }
    else if (input > _1B)
    {
        scaledInput /= _1B;
        scaledUnit = 'B';
    }
    else if (input > _1M)
    {
        scaledInput /= _1M;
        scaledUnit = 'M';
    }
    else if (input > _1K)
    {
        scaledInput /= _1K;
        scaledUnit = 'K';
    }
    char* buffer = (char*)malloc(sizeof(char) * 100);
    sprintf_s(buffer, 100, "%4.2f%c", scaledInput, scaledUnit);
    return buffer;
}

/// <summary>
/// Given a number 'input', each thread finds smallest prime number greater than 'n'.
/// If next prime number is beyond INT_MAX, it will return 0.
/// </summary>
/// <param name="n"></param>
/// <returns></returns>
ulong FindNextPrimeNumber(ulong input)
{
    // Start checking each number from input upto input * 2.
    for (ulong i = input; i < input * 2; i++)
    {
        bool found = true;
        for (ulong j = 2; j < i / 2; j++)
        {
            if (i % j == 0)
            {
                found = false;
                break;
            }
        }

        if (found)
        {
            return i;
        }
    }
    return 0;
}

/// <summary>
/// Threads will fetch a number from the queue and start the work to find the prime number.
/// Once all threads are done with their respective input, it will proceed to fetch next number.
/// </summary>
/// <param name="lpParam"></param>
/// <returns>status</returns>
DWORD WINAPI ThreadWorker(LPVOID lpParam)
{
    ThreadInput* tInput = (ThreadInput*)lpParam;

    // Make sure things are initialized correctly.
    assert(tInput->hardWaitCount == 0);
    assert(tInput->softWaitCount == 0);
    assert(tInput->totalIterations == 0);
    int threadId = tInput->threadId;
    int processedCount = 0;

    for (int i = 0; i < tInput->count; i++)
    {
        PRINT_PROGRESS("*** Processing: %u out of %u..", threadId, tInput->processed, tInput->count);
        ulong input = tInput->input[i];
        ulong answer = FindNextPrimeNumber(input);
        tInput->processed++;

        // So the compiler doesn't throw away answer and processedCount;
        tInput->answer |= answer;

        PRINT_ANSWER(" %u %llu= %llu", threadId, processedCount, input, answer);

        bool wasHardWait = false;
        unsigned __int64 spinLoopStopTime = 0;
        unsigned __int64 spinLoopStartTime = 0;

        tInput->totalIterations += joinData->join(i, threadId, &wasHardWait, &spinLoopStartTime , &spinLoopStopTime);

        // The last thread to complete will return here and "restart()".
        if (joinData->joined())
        {
            joinData->restart(tInput->threadId, i, tInput->processed == tInput->count);
        }
        else
        {
            // Even though we hard-wait, we also did spin-loop. See how much time was spent in that.
            unsigned __int64 softWaitCpuCycles = spinLoopStopTime - spinLoopStartTime;
            tInput->softWaitTimeTicks += softWaitCpuCycles;

            // Other threads that were waiting so long, will record the latency for
            // wakeup time as soon as things are restarted.
            if (wasHardWait)
            {
                unsigned __int64 hardWaitWakeupLatency = joinData->getTicksSinceRestart();
                tInput->hardWaitWakeupTimeTicks += hardWaitWakeupLatency;
                tInput->hardWaitCount++;

                PRINT_HARD_WAIT_LATENCY("%d. %lld cycles, %llu total spin-loop cycles", threadId, i, hardWaitWakeupLatency, softWaitCpuCycles);
            }
            else
            {
                unsigned __int64 softWaitWakeupLatency = joinData->getTicksSinceRestart();

                tInput->softWaitWakeupTimeTicks += softWaitWakeupLatency;
                tInput->softWaitCount++;

                PRINT_SOFT_WAIT_LATENCY("%d. %lld wake-up cycles, %llu total spin-loop cycles.", threadId, i, softWaitWakeupLatency, softWaitCpuCycles);
            }
        }
    }

    PRINT_PROGRESS("*** Total processed: %u out of %u..", threadId, processedCount, tInput->count);
    return 0;
}

class PrimeNumbers
{
private:
    int PROCESSOR_COUNT = -1, PROCESSOR_GROUP_COUNT, MWAITX_CYCLES, INPUT_COUNT, COMPLEXITY, JOIN_TYPE;

    void DiffWakeTime(ulong hardWaitWakeTime, ulong softWaitWakeTime, ulong* diff, char* diffCh)
    {
        *diffCh = ' ';
        *diff = hardWaitWakeTime - softWaitWakeTime;
        if (hardWaitWakeTime < softWaitWakeTime)
        {
            *diffCh = '-';
            *diff = softWaitWakeTime - hardWaitWakeTime;
        }
    }

    void parseArgs(int argc, char** argv)
    {
#define ARGS(argumentName)                  \
    int argumentName = 0;                   \
    bool argumentName##_used = false;

#define VALIDATE_AND_SET(paramName)                                 \
        if (_strcmpi(parameterName,"--" # paramName) == 0)          \
        {                                                           \
            if (paramName##_used)                                   \
            {                                                       \
                printf("--" # paramName ## "already specified.\n"); \
                PrintUsageAndExit();                                \
            }                                                       \
            paramName = atoi(parameterValue);                       \
            paramName##_used = true;                                \
            continue;                                               \
        }

        ARGS(input_count);
        ARGS(complexity);
        ARGS(thread_count);
        ARGS(mwaitx_cycle_count);
        ARGS(join_type);

        if (argc == 1)
        {
            PrintUsageAndExit();
        }

        for (int i = 1; i < argc; i++)
        {
            char* parameterName = argv[i++];
            if ((_strcmpi(parameterName, "-?") == 0) || (_strcmpi(parameterName, "-h") == 0) || (_strcmpi(parameterName, "-help") == 0))
            {
                PrintUsageAndExit();
            }

            if (i == argc)
            {
                printf("Missing value for parameter: '%s'\n", parameterName);
                PrintUsageAndExit();
            }
            char* parameterValue = argv[i];

            VALIDATE_AND_SET(input_count);
            VALIDATE_AND_SET(complexity);
            VALIDATE_AND_SET(thread_count);
            VALIDATE_AND_SET(join_type);
            VALIDATE_AND_SET(mwaitx_cycle_count);

            printf("Unknown parameter: '%s'\n", parameterName);
            PrintUsageAndExit();
        }

        complexity %= 32;

        // Verifications
        if (!input_count_used || !complexity_used)
        {
            printf("Missing mandatory arguments.\n");
            PrintUsageAndExit();
        }
        else
        {
            INPUT_COUNT = input_count;
            COMPLEXITY = complexity;
        }

        if (thread_count_used)
        {
            if (thread_count <= 0)
            {
                printf("Invalid value '%d' for '--thread_count'. Should be > 1.\n", thread_count);
                PrintUsageAndExit();
            }
            else
            {
                PROCESSOR_COUNT = thread_count;
            }
        }

        if (join_type_used)
        {
            if ((join_type < 1) || (join_type > 7))
            {
                printf("Invalid value '%d' for '--join_type'. Should be between 1 and 7.\n", join_type);
                PrintUsageAndExit();
            }
            else
            {
                JOIN_TYPE = join_type;
            }
        }
        else
        {
            JOIN_TYPE = 1;
        }

        if (mwaitx_cycle_count_used)
        {
            if ((join_type < 3) || (join_type > 6))
            {
                printf("Warning: '--mwaitx_cycle_count' is specified, but value will not be used for 'pause' wait type.\n");
            }
            else
            {
                MWAITX_CYCLES = mwaitx_cycle_count;
            }
        }
        else
        {
            if ((join_type >= 3) && (join_type <= 6))
            {
                printf("Warning: '--mwaitx_cycle_count' is needed when join_type is related to mwaitx.\n");
                PrintUsageAndExit();
            }
        }
    }

    void PrintUsageAndExit()
    {
        printf("\nUsage: PrimeNumbers.exe --input_count <numPrimeNumbers> --complexity <complexity> [options]\n");
        printf("<numPrimeNumbers>: Number of prime numbers per thread.\n");
        printf("<complexity>: Number between 0~31.\n");
        printf("\n");
        printf("Options:\n");
        printf("--thread_count <N>: Number of threads to use. By default it will use number of cores available in all groups.\n");
        printf("--mwaitx_cycle_count <N>: If specified, the number of cycles to pass in mwaitx().\n");
        printf("--join_type <N>\n");
        printf("  1= The current GC implementation [t_join_pause]\n");
        printf("  2= Use 'pause', only use in spin-loop, no hard-wait [t_join_pause_soft_wait_only]\n");
        printf("  3= Use 'mwaitx', use inside spin-loop [t_join_mwaitx_loop]\n");
        printf("  4= Use 'mwaitx', only use in spin-loop, no hard-wait [t_join_mwaitx_loop_soft_wait_only]\n");
        printf("  5= Use 'mwaitx', no spin-loop involved [t_join_mwaitx_noloop]\n");
        printf("  6= Use 'mwaitx', no spin-loop involved, no hard-wait [t_join_mwaitx_noloop_soft_wait_only]\n");
        printf("  7= Only hard-wait. [t_join_hard_wait_only]\n");
        exit(1);
    }

public:

    PrimeNumbers(int argc, char** argv)
    {
        parseArgs(argc, argv);

        int userInput_processor_count = PROCESSOR_COUNT;
        GetProcessorInfo(&PROCESSOR_COUNT, &PROCESSOR_GROUP_COUNT);
        if (userInput_processor_count != -1)
        {
            PROCESSOR_COUNT = userInput_processor_count;
        }

        PRINT_STATS("Running: SPIN_COUNT= %d, numbers= %d, complexity= %d, JOIN_TYPE= %d, threads= %d", SPIN_COUNT, INPUT_COUNT, COMPLEXITY, JOIN_TYPE, PROCESSOR_COUNT);
    }

    /// <summary>
    /// Given a number 'n', each thread finds smallest prime number greater than 'n'.
    /// If next prime number is beyond INT_MAX, it will return 0.
    /// 
    /// The test will produce `numPrimeNumbers` random numbers per thread and add in thread's queue.
    /// </summary>
    /// <param name="args"></param>
    /// <returns></returns>
    bool PrimeNumbersTest()
    {
        std::vector<HANDLE> threadHandles(PROCESSOR_COUNT);
        std::vector<DWORD> threadIds(PROCESSOR_COUNT);
        std::vector<ThreadInput*> threadInputs(PROCESSOR_COUNT);

        switch (JOIN_TYPE)
        {
        case 1:
            joinData = new t_join_pause(PROCESSOR_COUNT);
            break;
        case 2:
            joinData = new t_join_pause_soft_wait_only(PROCESSOR_COUNT);
            break;
        case 3:
            joinData = new t_join_mwaitx_loop(PROCESSOR_COUNT, MWAITX_CYCLES);
            break;
        case 4:
            joinData = new t_join_mwaitx_loop_soft_wait_only(PROCESSOR_COUNT, MWAITX_CYCLES);
            break;
        case 5:
            joinData = new t_join_mwaitx_noloop(PROCESSOR_COUNT, MWAITX_CYCLES);
            break;
        case 6:
            joinData = new t_join_mwaitx_noloop_soft_wait_only(PROCESSOR_COUNT, MWAITX_CYCLES);
            break;
        case 7:
            joinData = new t_join_hard_wait_only(PROCESSOR_COUNT);
            break;
        default:
            printf("");
            break;
        }

        // Create all the threads
        for (int i = 0; i < PROCESSOR_COUNT; i++)
        {
            ThreadInput* tInput = new ThreadInput(i, INPUT_COUNT);
            if (tInput != NULL)
            {
                float n;
                tInput->input = (ulong*)malloc((sizeof(ulong) * INPUT_COUNT));
                for (int i = 0; i < INPUT_COUNT; i++)
                {
                    if (COMPLEXITY == 0)
                    {
                        tInput->input[i] = i;
                    }
                    else
                    {
                        n = (float)rand() / RAND_MAX;
                        tInput->input[i] = (ulong)(n * (100 + pow(2, COMPLEXITY)));
                    }
                }
            }
            else {
                assert(!"Failed to allocate tInput");
            }

            DWORD tid;
            threadHandles[i] = CreateThread(
                NULL,                           // default security attributes
                0,                              // use default stack size
                ThreadWorker,                   // thread function name
                (LPVOID)tInput,                 // argument to thread function
                CREATE_SUSPENDED,
                &tid);                          // returns the thread identifier

            threadIds[i] = tid;
            threadInputs[i] = tInput;

            wchar_t buffer[15];
            wsprintf(buffer, L"Thread# %d", i);

            SetThreadDescription(threadHandles[i], buffer);
        }

        // Hard affinitize the threads to cores.
        SetThreadAffinity(PROCESSOR_COUNT, PROCESSOR_GROUP_COUNT > 1, threadHandles);

        // https://stackoverflow.com/a/27739925
        beginTimer = std::chrono::steady_clock::now();
        start = __rdtsc();

        // Start all the threads
        for (int i = 0; i < PROCESSOR_COUNT; i++)
        {
            ResumeThread(threadHandles[i]);
        }

        // Wait till last thread would signal that it is done
        joinData->waitForThreads();

        unsigned __int64 elapsed_ticks = __rdtsc() - start;
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beginTimer).count();

        int totalHardWaits = 0, totalSoftWaits = 0;
        ulong totalIterations = 0;
        unsigned __int64 totalSoftWaitWakeupTimeTicks = 0;
        unsigned __int64 totalHardWaitWakeupTimeTicks = 0;
        unsigned __int64 totalSpinLoopTime = 0;
        for (int i = 0; i < PROCESSOR_COUNT; i++)
        {
            char diffCh;
            ulong diff;
            ThreadInput* outputData = threadInputs[i];
            assert(outputData->hardWaitCount <= INPUT_COUNT);
            assert(outputData->softWaitCount <= INPUT_COUNT);
            totalHardWaits += outputData->hardWaitCount;
            totalSoftWaits += outputData->softWaitCount;
            totalIterations += outputData->totalIterations;
            totalSoftWaitWakeupTimeTicks += outputData->softWaitWakeupTimeTicks;
            totalHardWaitWakeupTimeTicks += outputData->hardWaitWakeupTimeTicks;
            totalSpinLoopTime += outputData->softWaitTimeTicks;
            DiffWakeTime(outputData->hardWaitWakeupTimeTicks, outputData->softWaitWakeupTimeTicks, &diff, &diffCh);
            PRINT_THEAD_STATS("[Thread #%d] Iterations: %llu, HardWait: %d, SoftWait: %d, SpinLoop cycles: %llu, HardWaitWakeupTime: %llu, SoftWaitWakeupTime: %llu, Diff: %c%llu", i, outputData->totalIterations, outputData->hardWaitCount, outputData->softWaitCount, outputData->softWaitTimeTicks, outputData->hardWaitWakeupTimeTicks, outputData->softWaitWakeupTimeTicks, diffCh, diff);
        }


#define AVG(n) ((n / (INPUT_COUNT * PROCESSOR_COUNT)) + 1)
#define AVG_NUMBER(n) ((n / INPUT_COUNT) + 1)
#define AVG_THREAD(n) ((n / PROCESSOR_COUNT) + 1)

#define AVG_WAKETIME(n, count) ((n / count) + 1)

        ulong avgDiff;
        char avgDiffChar;

        ulong avgHardWaitWakeupTime = totalHardWaits == 0 ? 0 : AVG_WAKETIME(totalHardWaitWakeupTimeTicks, totalHardWaits);
        ulong avgSoftWaitWakeupTime = totalSoftWaits == 0 ? 0 : AVG_WAKETIME(totalSoftWaitWakeupTimeTicks, totalSoftWaits);
        // We spin-loop for both, hard-wait and soft-wait. So take both into account.
        ulong avgSpinLoopTime = (totalHardWaits + totalSoftWaits) == 0 ? 0 : AVG_WAKETIME(totalSpinLoopTime, (totalHardWaits + totalSoftWaits));

        DiffWakeTime(avgHardWaitWakeupTime, avgSoftWaitWakeupTime, &avgDiff, &avgDiffChar);

        PRINT_STATS("...........................................................");
        PRINT_STATS("Latency numbers: HardWait: %s, SoftWait: %s, AvgSpinLoopTimePerWait: %s, TotalSpinWasteTime: %s, HardWaitWakeupTime: %s, SoftWaitWakeupTime: %s, Diff: %c%s", formatNumber(totalHardWaits), formatNumber(totalSoftWaits), formatNumber(avgSpinLoopTime), formatNumber(totalSpinLoopTime), formatNumber(avgHardWaitWakeupTime), formatNumber(avgSoftWaitWakeupTime), avgDiffChar, formatNumber(avgDiff));
        PRINT_STATS("...........................................................");
        PRINT_STATS("Average per input_number: Iterations: %s, HardWait: %s, SoftWait: %s", formatNumber(AVG(totalIterations)), formatNumber(AVG(totalHardWaits)), formatNumber(AVG(totalSoftWaits)));
        PRINT_STATS("Average per input_number (all threads): Iterations: %s, HardWait: %s, SoftWait: %s", formatNumber(AVG_NUMBER(totalIterations)), formatNumber(AVG_NUMBER(totalHardWaits)), formatNumber(AVG_NUMBER(totalSoftWaits)));
        PRINT_STATS("Average per thread ran  (all iterations): Iterations: %s, HardWait: %s, SoftWait: %s", formatNumber(AVG_THREAD(totalIterations)), formatNumber(AVG_THREAD(totalHardWaits)), formatNumber(AVG_THREAD(totalSoftWaits)));
        PRINT_STATS("Time taken: %llu ticks", elapsed_ticks);
        PRINT_STATS("Time difference = %lld milliseconds", elapsed_time);

        PRINT_ONELINE_STATS("OUT] %d|%d|%d|%llu|%d|%d|%llu|%llu|%llu|%llu|%d|%d|%llu|%llu|%llu|%llu|%d|%d|%llu|%llu|%llu|%llu|%llu",
            numPrimeNumbers, complexity, PROCESSOR_COUNT,
            AVG(totalIterations), AVG(totalHardWaits), AVG(totalSoftWaits), avgSpinLoopTime, avgHardWaitWakeupTime, avgSoftWaitWakeupTime,
            AVG_NUMBER(totalIterations), AVG_NUMBER(totalHardWaits), AVG_NUMBER(totalSoftWaits), avgSpinLoopTime_Number, avgHardWaitWakeupTime_Number, avgSoftWaitWakeupTime_Number,
            AVG_THREAD(totalIterations), AVG_THREAD(totalHardWaits), AVG_THREAD(totalSoftWaits), avgSpinLoopTime_Thread, avgHardWaitWakeupTime_Thread, avgSoftWaitWakeupTime_Thread,
            elapsed_ticks, elapsed_time);

        return true;
    }
};

int main(int argc, char** argv)
{
    PrimeNumbers p(argc, argv);
    p.PrimeNumbersTest();

    fflush(stdout);
    return 0;
}