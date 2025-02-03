#include "riteg/precompiled.hh"
#include "riteg/timings.hh"

void Timings::setup(Timings &timings)
{
    timings.curtime = glfwGetTime();
    timings.frametime = 1.0f / 60.0f;
    timings.framerate = 60.0f;
    timings.frame_number = 0;
}

void Timings::update(Timings &timings)
{
    float curtime = glfwGetTime();
    timings.frametime = curtime - timings.curtime;
    timings.framerate = 1.0f / timings.frametime;
    timings.curtime = curtime;
    timings.frame_number += 1;
}
