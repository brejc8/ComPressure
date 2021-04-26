#pragma once
#include "Misc.h"
#include "SaveState.h"
#include "Circuit.h"
#include "Demo.h"
#include <stdlib.h>


#define LEVEL_COUNT 33
#define HISTORY_POINT_COUNT 200


class SimPoint
{
public:
    unsigned values[4] = {0};
    unsigned force[4] = {50};
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

    unsigned first_simpoint = 0;
    bool reset_1 = false;
    bool reset_all = false;
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
    std::string name = "";
    CircuitPressure ports[4];
    SimPoint current_simpoint;
    TestExecType monitor_state = MONITOR_STATE_PLAY_ALL;

    unsigned level_index;
    bool hidden = false;
    Circuit* circuit;
    LevelSet *best_design = NULL;
    LevelSet *saved_designs[4] = {NULL};

    int pin_order[4] = {-1};

    unsigned connection_mask = 0;
    
    unsigned substep_count = 10000;
    
    std::vector<Test> tests;
    
    
    Pressure best_score = 0;
    Pressure last_score = 0;
    unsigned level_version = 0;
    
    
    bool touched = false;
    bool score_set = false;
    bool best_score_set = false;

    unsigned test_index = 0;
    unsigned sim_point_index = 0;
    unsigned substep_index = 0;

    class PressureRecord
    {
    public:
        Pressure values[4] = {-1};
    } test_pressure_histroy[192];
    int test_pressure_histroy_index = 0;
    int test_pressure_histroy_sample_downcounter = 0;

    class FriendScore
    {
    public:
        std::string steam_username;
        uint64_t steam_id;
        Pressure score;
    };
    std::vector<FriendScore> friend_scores;


    Pressure global_score_graph[200];
    Pressure global_fetched_score;
    bool global_score_graph_set = false;
    unsigned global_score_graph_time = 0;

    Level(unsigned level_index_, SaveObject* sobj);
    Level(unsigned level_index_, bool hidden_ = false);
    ~Level();
    SaveObject* save(bool lite = false);

    XYPos getimage(DirFlip dir_flip);
    XYPos getimage_fg(DirFlip dir_flip);

    void init_tests(SaveObjectMap* omap = NULL);
    void reset();
    void advance(unsigned ticks);
    void select_test(unsigned t);

    void update_score(bool fin);
    void set_monitor_state(TestExecType monitor_state_);
    void touch();

};


class LevelSet
{
public:
    Level* levels[LEVEL_COUNT+1];
    bool read_only = false;
    LevelSet(SaveObject* sobj, bool inspect = false);
    LevelSet();
    ~LevelSet();
    SaveObject* save_all(unsigned level_index, bool lite = false);
    SaveObject* save_one(unsigned level_index);
    bool is_playable(unsigned level, unsigned highest_level);
    int top_playable();
    Pressure test_level(unsigned level_index);
    void record_best_score(unsigned level_index);
    void save_design(unsigned level_index, unsigned save_slot);
    void reset(unsigned level_index);
    void remove_circles(unsigned level_index);
    void touch(unsigned level_index);

};
