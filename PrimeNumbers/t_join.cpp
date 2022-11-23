#include <intrin.h>
#include "common.h"
#include "t_join.h"

ulong t_join_pause::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
respin:
            int j = 0;
            for (; j < SPIN_COUNT; j++)
            {
                if (color != join_struct.lock_color.LoadWithoutBarrier())
                {
                    totalIterations += j;

                    PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    break;
                }
                YieldProcessor();
            }

            if (j == SPIN_COUNT)
            {
                totalIterations += SPIN_COUNT;
            }

            HARD_WAIT();
        }
    }
    else
    {
        RESET_HARD_WAIT();
    }
    return totalIterations;
}

ulong t_join_pause2::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
        respin:
            int j = 0;
            for (; j < SPIN_COUNT; j++)
            {
                if (color != join_struct.lock_color.LoadWithoutBarrier())
                {
                    totalIterations += j;

                    PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    break;
                }
                YieldProcessor();
                YieldProcessor();
            }

            if (j == SPIN_COUNT)
            {
                totalIterations += SPIN_COUNT;
            }

            HARD_WAIT();
        }
    }
    else
    {
        RESET_HARD_WAIT();
    }
    return totalIterations;
}

ulong t_join_pause10::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
        respin:
            int j = 0;
            for (; j < SPIN_COUNT; j++)
            {
                if (color != join_struct.lock_color.LoadWithoutBarrier())
                {
                    totalIterations += j;

                    PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    break;
                }
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
                YieldProcessor();
            }

            if (j == SPIN_COUNT)
            {
                totalIterations += SPIN_COUNT;
            }

            HARD_WAIT();
        }
    }
    else
    {
        RESET_HARD_WAIT();
    }
    return totalIterations;
}


ulong t_join_no_pause::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
        respin:
            int j = 0;
            for (; j < SPIN_COUNT; j++)
            {
                if (color != join_struct.lock_color.LoadWithoutBarrier())
                {
                    totalIterations += j;

                    PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    break;
                }
            }

            if (j == SPIN_COUNT)
            {
                totalIterations += SPIN_COUNT;
            }

            HARD_WAIT();
        }
    }
    else
    {
        RESET_HARD_WAIT();
    }
    return totalIterations;
}

ulong t_join_mwaitx_noloop::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
respin:
            _mm_monitorx((const void*)&join_struct.lock_color, 0, 0);
            _mm_mwaitx(2, 0, mwaitx_cycles);
            totalIterations += 1;

            HARD_WAIT();
        }
    }
    else
    {
        RESET_HARD_WAIT();
    }
    return totalIterations;
}

ulong t_join_mwaitx_loop::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
respin:
            int j = 0;
            for (; j < SPIN_COUNT; j++)
            {
                _mm_monitorx((const void*)&join_struct.lock_color, 0, 0);
                if (color != join_struct.lock_color.LoadWithoutBarrier())
                {
                    totalIterations += j;

                    PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    break;
                }
                _mm_mwaitx(2, 0, mwaitx_cycles);
            }

            if (j == SPIN_COUNT)
            {
                totalIterations += SPIN_COUNT;
            }

            HARD_WAIT();
        }
    }
    else
    {
        RESET_HARD_WAIT();
    }
    return totalIterations;
}

ulong t_join_hard_wait_only::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
respin:

            HARD_WAIT();
        }
    }
    else
    {
        RESET_HARD_WAIT();
    }
    return totalIterations;
}

ulong t_join_pause_soft_wait_only::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
respin:
            int j = 0;
            for (; j < SPIN_COUNT; j++)
            {
                if (color != join_struct.lock_color.LoadWithoutBarrier())
                {
                    totalIterations += j;

                    PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    break;
                }
                YieldProcessor();           // indicate to the processor that we are spinning
            }

            if (j == SPIN_COUNT)
            {
                totalIterations += SPIN_COUNT;
            }

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
    }
    return totalIterations;
}

ulong t_join_mwaitx_loop_soft_wait_only::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
            *spinLoopStartTime = GetCounter();
respin:
            int j = 0;
            for (; j < SPIN_COUNT; j++)
            {
                _mm_monitorx((const void*)&join_struct.lock_color, 0, 0);
                if (color != join_struct.lock_color.LoadWithoutBarrier())
                {
                    totalIterations += j;

                    PRINT_SOFT_WAIT("%d. %llu iterations.", threadId, inputIndex, totalIterations);
                    break;
                }
                _mm_mwaitx(2, 0, mwaitx_cycles);
            }

            if (j == SPIN_COUNT)
            {
                totalIterations += SPIN_COUNT;
            }

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
    }
    return totalIterations;
}

ulong t_join_mwaitx_noloop_soft_wait_only::join(int inputIndex, int threadId, bool* wasHardWait, unsigned __int64* spinLoopStartTime, unsigned __int64* spinLoopStopTime)
{
    ulong totalIterations = 0;
    *wasHardWait = false;
    int color = join_struct.lock_color.LoadWithoutBarrier();
    if (_InterlockedDecrement((long*)&join_struct.join_lock) != 0)
    {
        if (color == join_struct.lock_color.LoadWithoutBarrier())
        {
respin:
            _mm_monitorx((const void*)&join_struct.lock_color, 0, 0);
            _mm_mwaitx(2, 0, mwaitx_cycles);

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
    }
    return totalIterations;
}