// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef _WIN32
#include <time.h>
#endif //_WIN32

// Helper class for animation and simulation timing.
class FStepTimer
{
public:
    FStepTimer() noexcept(false) :
        ElapsedTicks(0),
        TotalTicks(0),
        LeftOverTicks(0),
        FrameCount(0),
        FramesPerSecond(0),
        FramesThisSecond(0),
        bIsFixedTimeStep(false),
        TargetElapsedTicks(TicksPerSecond / 60)
    {
#ifdef DXTK
        if (!QueryPerformanceFrequency(&QPCFrequency))
        {
            throw std::runtime_error( "QueryPerformanceFrequency" );
        }

        if (!QueryPerformanceCounter(&QCPLastTime))
        {
            throw std::runtime_error( "QueryPerformanceCounter" );
        }

		QPCSecondCounter = 0;

        // Initialize max delta to 1/10 of a second.
        MaxDelta = static_cast<uint64_t>(QPCFrequency.QuadPart / 10);
#endif //DXTK

#ifdef EOS_DEMO_SDL
		MaxDelta = 100; //100 ms
		LastTime = 0;
#endif //EOS_DEMO_SDL
    }

    // Get elapsed time since the previous Update call.
    uint64_t GetElapsedTicks() const					{ return ElapsedTicks; }
    double GetElapsedSeconds() const					{ return TicksToSeconds(ElapsedTicks); }

    // Get total time since the start of the program.
    uint64_t GetTotalTicks() const						{ return TotalTicks; }
    double GetTotalSeconds() const						{ return TicksToSeconds(TotalTicks); }

    // Get total number of updates since start of the program.
    uint32_t GetFrameCount() const						{ return FrameCount; }

    // Get the current framerate.
    uint32_t GetFramesPerSecond() const					{ return FramesPerSecond; }

    // Set whether to use fixed or variable timestep mode.
    void SetFixedTimeStep(bool bInIsFixedTimestep)			{ bIsFixedTimeStep = bInIsFixedTimestep; }

    // Set how often to call Update when in fixed timestep mode.
    void SetTargetElapsedTicks(uint64_t InTargetElapsed)	{ TargetElapsedTicks = InTargetElapsed; }
    void SetTargetElapsedSeconds(double InTargetElapsed)	{ TargetElapsedTicks = SecondsToTicks(InTargetElapsed); }

    // Integer format represents time using 10,000,000 ticks per second.
#ifdef DXTK
    static const uint64_t TicksPerSecond = 10000000;
#endif //DXTK
#ifdef EOS_DEMO_SDL
	static const uint64_t TicksPerSecond = 1000;
#endif //EOS_DEMO_SDL

    static double TicksToSeconds(uint64_t Ticks)		{ return static_cast<double>(Ticks) / TicksPerSecond; }
    static uint64_t SecondsToTicks(double Seconds)		{ return static_cast<uint64_t>(Seconds * TicksPerSecond); }

    // After an intentional timing discontinuity (for instance a blocking IO operation)
    // call this to avoid having the fixed timestep logic attempt a set of catch-up 
    // Update calls.

    void ResetElapsedTime()
    {
#ifdef DXTK
        if (!QueryPerformanceCounter(&QCPLastTime))
        {
            throw std::exception("QueryPerformanceCounter");
        }
		QPCSecondCounter = 0;
#endif //DXTK

        LeftOverTicks = 0;
        FramesPerSecond = 0;
        FramesThisSecond = 0;
    }

    // Update timer state, calling the specified Update function the appropriate number of times.
    template<typename TUpdate>
    void Tick(const TUpdate& Update)
    {
#ifdef DXTK
        // Query the current time.
        LARGE_INTEGER CurrentTime;

        if (!QueryPerformanceCounter(&CurrentTime))
        {
            throw std::exception( "QueryPerformanceCounter" );
        }

        uint64_t TimeDelta = CurrentTime.QuadPart - QCPLastTime.QuadPart;

        QCPLastTime = CurrentTime;
        QPCSecondCounter += TimeDelta;
#endif //DXTK

#ifdef EOS_DEMO_SDL
		unsigned int CurrentTime = SDL_GetTicks();
		unsigned int TimeDelta = CurrentTime - LastTime;
		LastTime = CurrentTime;
#endif //EOS_DEMO_SDL

        // Clamp excessively large time deltas (e.g. after paused in the debugger).
        if (TimeDelta > MaxDelta)
        {
            TimeDelta = MaxDelta;
        }


#ifdef DXTK
		// Convert QPC units into a canonical tick format. This cannot overflow due to the previous clamp.
		TimeDelta *= TicksPerSecond;
        TimeDelta /= QPCFrequency.QuadPart;
#endif //DXTK

        uint32_t LastFrameCount = FrameCount;

        if (bIsFixedTimeStep)
        {
            // Fixed timestep update logic

            // If the app is running very close to the target elapsed time (within 1/4 of a millisecond) just clamp
            // the clock to exactly match the target value. This prevents tiny and irrelevant errors
            // from accumulating over time. Without this clamping, a game that requested a 60 fps
            // fixed update, running with vsync enabled on a 59.94 NTSC display, would eventually
            // accumulate enough tiny errors that it would drop a frame. It is better to just round 
            // small deviations down to zero to leave things running smoothly.

            if (static_cast<uint64_t>(std::abs(static_cast<int64_t>(TimeDelta - TargetElapsedTicks))) < TicksPerSecond / 4000)
            {
#ifdef DXTK
                TimeDelta = TargetElapsedTicks;
#endif

#ifdef EOS_DEMO_SDL
				TimeDelta = static_cast<unsigned int>(TargetElapsedTicks);
#endif
            }

            LeftOverTicks += TimeDelta;

            while (LeftOverTicks >= TargetElapsedTicks)
            {
                ElapsedTicks = TargetElapsedTicks;
                TotalTicks += TargetElapsedTicks;
                LeftOverTicks -= TargetElapsedTicks;
                FrameCount++;

                Update();
            }
        }
        else
        {
            // Variable timestep update logic.
            ElapsedTicks = TimeDelta;
            TotalTicks += TimeDelta;
            LeftOverTicks = 0;
            FrameCount++;

            Update();
        }

        // Track the current framerate.
        if (FrameCount != LastFrameCount)
        {
            FramesThisSecond++;
        }

#ifdef DXTK
        if (QPCSecondCounter >= static_cast<uint64_t>(QPCFrequency.QuadPart))
        {
            FramesPerSecond = FramesThisSecond;
            FramesThisSecond = 0;
            QPCSecondCounter %= QPCFrequency.QuadPart;
        }
#endif //DXTK

#ifdef EOS_DEMO_SDL
		if ((CurrentTime / 1000) != ((CurrentTime - TimeDelta) / 1000))
		{
			FramesPerSecond = FramesThisSecond;
			FramesThisSecond = 0;
		}
#endif //EOS_DEMO_SDL

    }

private:
    // Source timing data uses QPC units.
#ifdef DXTK
    LARGE_INTEGER QPCFrequency;
    LARGE_INTEGER QCPLastTime;
	uint64_t QPCSecondCounter;
	uint64_t MaxDelta;
#endif //DXTK

#ifdef EOS_DEMO_SDL
	unsigned int LastTime;
	unsigned int MaxDelta;
#endif //EOS_DEMO_SDL

    // Derived timing data uses a canonical tick format.
    uint64_t ElapsedTicks;
    uint64_t TotalTicks;
    uint64_t LeftOverTicks;

    // Members for tracking the framerate.
    uint32_t FrameCount;
    uint32_t FramesPerSecond;
    uint32_t FramesThisSecond;

    // Members for configuring fixed timestep mode.
    bool bIsFixedTimeStep;
    uint64_t TargetElapsedTicks;
};
