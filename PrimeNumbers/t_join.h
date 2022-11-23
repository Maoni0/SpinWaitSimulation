#pragma once
#include <windows.h>
#include <chrono>
#include "common.h"
#include "Volatile.h"

struct join_structure
{
    // Shared non volatile keep on separate line to prevent eviction
    int n_threads;
    EventImpl joined_event[3];
    Volatile<int> lock_color;
    Volatile<bool> wait_done;
    Volatile<bool> joined_p;
    Volatile<int> join_lock;
    unsigned __int64 restartStartTime;
};

__forceinline LONGLONG GetCounter()
{
    //LARGE_INTEGER time;
    //BOOL result1 = QueryPerformanceFrequency(&time);
    //BOOL result = QueryPerformanceCounter(&time);
    //if (result == 0)
    //{
    //    printf("QueryPerformanceCounter returned error (%d). GetLastError() = %u\n", result, GetLastError());
    //    exit(1);
    //}
    //return time.QuadPart;
    unsigned int cpuNum;
    return __rdtscp(&cpuNum);
}

class t_join
{
private:
    EventImpl waitToComplete;

protected:
    join_structure join_struct;

    t_join(int numThreads)
    {
        join_struct.n_threads = numThreads;
        join_struct.lock_color = 0;
        for (int i = 0; i < 3; i++)
        {
            if (!join_struct.joined_event[i].IsValid())
            {
                join_struct.joined_p = FALSE;

                if (!join_struct.joined_event[i].CreateManualEvent(false))
                {
                    break;
                }
            }
        }
        join_struct.join_lock = join_struct.n_threads;
        join_struct.wait_done = false;
        join_struct.restartStartTime = 0;

        // Create an event to wait for all threads to complete.
        waitToComplete.CreateManualEvent(false);
    }

    ulong old_join(int inputIndex, int threadId, bool* wasHardWait)
    {
        ulong totalIterations = 0;
        *wasHardWait = false;
        int color = join_struct.lock_color.LoadWithoutBarrier();
        if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
        {
            if (color == join_struct.lock_color.LoadWithoutBarrier())
            {
            respin:
#ifdef USE_MWAITX_NOLOOP
                _mm_monitorx((const void*)&join_struct.lock_color, 0, 0);
                _mm_mwaitx(2, 0, MWAITX_CYCLES);
                totalIterations += 1;
                //auto before_waitx = __rdtsc();
                ////_mm_monitorx((const void*)&totalIterations, 0, 0);
                //_mm_monitorx((const void*)&join_struct.lock_color, 0, 0);
                //_mm_mwaitx(2, 0, MWAITX_CYCLES);
                //auto after_waitx = __rdtsc();
                //printf("actual: %u, MWAITX_CYCLES: %d\n", (after_waitx - before_waitx), MWAITX_CYCLES);
#endif
#ifndef SKIP_SOFT_WAIT
                int j = 0;
                for (; j < SPIN_COUNT; j++)
                {
#ifdef USE_MWAITX
                    _mm_monitorx((const void*)&join_struct.lock_color, 0, 0);
#endif // USE_MWAITX
                    if (color != join_struct.lock_color.LoadWithoutBarrier())
                    {
                        PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                        break;
                    }
#ifdef USE_PAUSE
                    YieldProcessor();           // indicate to the processor that we are spinning
#endif // USE_PAUSE

#ifdef USE_MWAITX
                    _mm_mwaitx(2, 0, MWAITX_CYCLES);
#endif // USE_MWAITX
                }
                totalIterations += j;
#endif // SKIP_SOFT_WAIT

#ifndef SKIP_HARD_WAIT
                // we've spun, and if color still hasn't changed, fall into hard wait
                if (color == join_struct.lock_color.LoadWithoutBarrier())
                {
                    PRINT_HARD_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    *wasHardWait = true;
                    uint32_t dwJoinWait = join_struct.joined_event[color].Wait(INFINITE, FALSE);

                    if (dwJoinWait != WAIT_OBJECT_0)
                    {
                        printf("Fatal error");
                        exit(1);
                    }
                }
#endif // SKIP_HARD_WAIT

                // avoid race due to the thread about to reset the event (occasionally) being preempted before ResetEvent()
                if (color == join_struct.lock_color.LoadWithoutBarrier())
                {
                    goto respin;
                }
            }
        }
        else
        {
            PRINT_RELEASE("%d", threadId, inputIndex);
            PRINT_RELEASE("---------------\n", threadId);
            join_struct.joined_p = true;
            join_struct.joined_event[!color].Reset();
        }
        return totalIterations;
    }

public:
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime) = 0;

    void waitForThreads()
    {
        uint32_t dwJoinWait = waitToComplete.Wait(INFINITE, FALSE);

        if (dwJoinWait != WAIT_OBJECT_0)
        {
            DWORD error = GetLastError();
            printf("WaitForMultipleObjects() failed with error code %d and return code %d\n", error, dwJoinWait);
            exit(1);
        }
    }

    __forceinline void restart(int threadId, int numberIndex, bool isLastIteration)
    {
        //printf("%d. Restart for Thread# %d\n", numberIndex, threadId);
        join_struct.joined_p = false;
        join_struct.join_lock = join_struct.n_threads;
        int color = join_struct.lock_color.LoadWithoutBarrier();

        // Record the start of "wake up time". Do this before changing
        // the color so we don't have a thread who reaches the place that
        // measures the "wakeupTimeTicks" before we even record the start
        // of "restart time".
        recordRestartStartTime();
        join_struct.lock_color = !color;
        join_struct.joined_event[color].Set();

        if (isLastIteration)
        {
            waitToComplete.Set();
        }
    }

    __forceinline void recordRestartStartTime()
    {
        join_struct.restartStartTime = GetCounter();
    }
    __forceinline unsigned __int64 getTicksSinceRestart()
    {
        assert(join_struct.restartStartTime != 0);
        return  GetCounter() - join_struct.restartStartTime;
    }

    bool joined()
    {
        return join_struct.joined_p;
    }
    
#define HARD_WAIT()                                                                     \
    *spinLoopStopTime = GetCounter();                                                   \
                                                                                        \
    /* we've spun, and if color still hasn't changed, fall into hard wait */            \
    if (color == join_struct.lock_color.LoadWithoutBarrier())                           \
    {                                                                                   \
        PRINT_HARD_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations); \
        *wasHardWait = true;                                                            \
        uint32_t dwJoinWait = join_struct.joined_event[color].Wait(INFINITE, FALSE);    \
                                                                                        \
        if (dwJoinWait != WAIT_OBJECT_0)                                                \
        {                                                                               \
            printf("Fatal error");                                                      \
            exit(1);                                                                    \
        }                                                                               \
    }                                                                                   \
                                                                                        \
    /* avoid race due to the thread about to reset the event (occasionally) being*/     \
    /* preempted before ResetEvent() */                                                 \
    if (color == join_struct.lock_color.LoadWithoutBarrier())                           \
    {                                                                                   \
        goto respin;                                                                    \
    }

#define RESET_HARD_WAIT()                           \
    PRINT_RELEASE("%d", threadId, inputIndex);      \
    PRINT_RELEASE("---------------\n", threadId);   \
    join_struct.joined_p = true;                    \
    join_struct.joined_event[!color].Reset();


};

class t_join_pause : public t_join
{
public:
    t_join_pause(int numThreads) : t_join(numThreads)
    {
    }

    /// <summary>
    /// Use the default implementation of "join" where we use "pause"
    /// inside the spin-loop and if after X spin iterations, color doesn't
    /// change, we would go to hard-wait.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_pause2 : public t_join
{
public:
    t_join_pause2(int numThreads) : t_join(numThreads)
    {
    }

    /// <summary>
    /// Use the default implementation of "join" where we use "pause"
    /// inside the spin-loop and if after X spin iterations, color doesn't
    /// change, we would go to hard-wait.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_pause10 : public t_join
{
public:
    t_join_pause10(int numThreads) : t_join(numThreads)
    {
    }

    /// <summary>
    /// Use the default implementation of "join" where we use "pause"
    /// inside the spin-loop and if after X spin iterations, color doesn't
    /// change, we would go to hard-wait.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_no_pause : public t_join
{
public:
    t_join_no_pause(int numThreads) : t_join(numThreads)
    {
    }

    /// <summary>
    /// Use the default implementation of "join" where we use "pause"
    /// inside the spin-loop and if after X spin iterations, color doesn't
    /// change, we would go to hard-wait.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};


class t_join_mwaitx_noloop : public t_join
{
private:
    const int mwaitx_cycles;

public:
    t_join_mwaitx_noloop(int numThreads, int mwaitx_timeout) : t_join(numThreads), mwaitx_cycles(mwaitx_timeout)
    {
    }

    /// <summary>
    /// Use monitorx/mwaitx without doing spin-loop. If the timeout
    /// expires and the color hasn't change, it will go to hard-wait.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_mwaitx_loop : public t_join
{
private:
    const int mwaitx_cycles;

public:
    t_join_mwaitx_loop(int numThreads, int mwaitx_timeout) : t_join(numThreads), mwaitx_cycles(mwaitx_timeout)
    {
    }

    /// <summary>
    /// Use monitorx/mwaitx inside spin-loop. If the timeout
    /// expires and the color hasn't change, it will go to hard-wait.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_hard_wait_only : public t_join
{
public:
    t_join_hard_wait_only(int numThreads) : t_join(numThreads)
    {
    }

    /// <summary>
    /// Only hard-wait for color change.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <param name="spinLoopStopTime">Time tick at which spin loop was completed
    /// (either because color was changed sooner, or it had to hard-wait, in which case
    /// it utilized all the spin iterations.</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_pause_soft_wait_only : public t_join
{
public:
    t_join_pause_soft_wait_only(int numThreads) : t_join(numThreads)
    {
    }

    /// <summary>
    /// Only uses spin-loop for color change, with "pause" inside every iteration.
    /// No hard-wait is done.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_mwaitx_loop_soft_wait_only : public t_join
{
private:
    const int mwaitx_cycles;

public:
    t_join_mwaitx_loop_soft_wait_only(int numThreads, int mwaitx_timeout) : t_join(numThreads), mwaitx_cycles(mwaitx_timeout)
    {
    }

    /// <summary>
    /// Only uses spin-loop for color change, with "mwaitx" inside every iteration.
    /// No hard-wait is done.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};

class t_join_mwaitx_noloop_soft_wait_only : public t_join
{
private:
    const int mwaitx_cycles;

public:
    t_join_mwaitx_noloop_soft_wait_only(int numThreads, int mwaitx_timeout) : t_join(numThreads), mwaitx_cycles(mwaitx_timeout)
    {
    }

    /// <summary>
    /// Only uses "mwaitx" with TIMEOUT inside every iteration.
    /// No hard-wait is done.
    /// </summary>
    /// <param name="inputIndex">index for which join is performed.</param>
    /// <param name="threadId">Thread id</param>
    /// <param name="wasHardWait">If there was hardwait needed</param>
    /// <returns>Total spin iterations performed.</returns>
    virtual ulong join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime);
};
