#ifndef RITEG_TIMINGS_HH
#define RITEG_TIMINGS_HH 1
#pragma once

class Timings final {
public:
    float curtime;
    float frametime;
    float framerate;
    int frame_number;

public:
    static void setup(Timings &timings);
    static void update(Timings &timings);
};

#endif /* RITEG_TIMINGS_HH */
