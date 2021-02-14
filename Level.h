#pragma once
#include "Misc.h"
#include "SaveState.h"
#include "Circuit.h"
#include "Demo.h"
#include <stdlib.h>


#ifdef COMPRESSURE_DEMO
#define LEVEL_COUNT 12
#else
#define LEVEL_COUNT 12
#endif
#define HISTORY_POINT_COUNT 200


class SimPoint
{
public:
    unsigned values[4] = {50};
    unsigned force[4] = {50};
    SimPoint(unsigned N, unsigned E, unsigned S, unsigned W)
    {
        values[0] = N;
        values[1] = E;
        values[2] = S;
        values[3] = W;
        force[0] = 50;
        force[1] = 50;
        force[2] = 50;
        force[3] = 50;

    }
    SimPoint(unsigned N, unsigned E, unsigned S, unsigned W, unsigned NF, unsigned EF, unsigned SF, unsigned WF)
    {
        values[0] = N;
        values[1] = E;
        values[2] = S;
        values[3] = W;
        force[0] = NF;
        force[1] = EF;
        force[2] = SF;
        force[3] = WF;
    }
    SimPoint()
    {
        assert(0);
    }
};

enum TestExecType
{
    MONITOR_STATE_PAUSE,
    MONITOR_STATE_PLAY_1,
    MONITOR_STATE_PLAY_ALL
};

class Test
{
public:
    Pressure last_score = 0;
    Pressure best_score = 0;
    Direction tested_direction = DIRECTION_E;

    std::vector<SimPoint> sim_points;
    Pressure best_pressure_log[HISTORY_POINT_COUNT];
    Pressure last_pressure_log[HISTORY_POINT_COUNT];
    unsigned last_pressure_index;

    Test();
    void load(SaveObject* sobj);
    SaveObject* save();
};

class Level
{
public:
    const char* name;
    CircuitPressure ports[4];

    unsigned level_index;
    Circuit* circuit;
    
    unsigned pin_order[4];

    unsigned connection_mask = 0;
    
    unsigned substep_count = 10000;
    
    std::vector<Test> tests;
    
    
    Pressure best_score = 0;
    Pressure last_score = 0;
    unsigned level_version = 0;
    bool enabled = false;
    
    
    bool touched = false;
    unsigned test_index;
    unsigned sim_point_index;
    unsigned substep_index;
    unsigned in_range_count;
    
    Level(unsigned level_index_, SaveObject* sobj);
    Level(unsigned level_index_);
    ~Level();
    SaveObject* save();

    XYPos getimage(Direction direction);
    XYPos getimage_fg(Direction direction);

    void init_tests(SaveObjectMap* omap = NULL);
    void reset(LevelSet* level_set);
    void advance(unsigned ticks, TestExecType type);
    void select_test(unsigned t);

    void update_score();

};


class LevelSet
{
public:
    Level* levels[LEVEL_COUNT];
    LevelSet(SaveObject* sobj);
    LevelSet();
    ~LevelSet();
    SaveObject* save();
    bool is_playable(unsigned level);
    int top_playable();


};
