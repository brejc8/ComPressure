#pragma once
#include "Misc.h"
#include "SaveState.h"
#include "Circuit.h"
#include "Demo.h"
#include <stdlib.h>


#define LEVEL_COUNT 40
#define HISTORY_POINT_COUNT 200

extern SaveObjectList* level_desc;

class WrappedTexture
{
public:
    virtual ~WrappedTexture(){};
    virtual SDL_Texture* get_texture () = 0;
};


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

enum TestResetType
{
    RESET_NONE,
    RESET_1,
    RESET_ALL
};

class Test
{
public:
    Pressure last_score = 0;
    Pressure best_score = 0;
    Direction tested_direction = DIRECTION_E;

    unsigned first_simpoint = 0;
    TestResetType reset = RESET_NONE;
    std::vector<SimPoint> sim_points;
    Pressure best_pressure_log[HISTORY_POINT_COUNT];
    Pressure last_pressure_log[HISTORY_POINT_COUNT];
    unsigned last_pressure_index;

    Test();
    void load(SaveObjectMap* player_map, SaveObjectMap* test_map);
    SaveObject* save(bool custom, bool lite);
};

class Level
{
public:

    uint8_t icon_pixels_fg[24][24] = {0};
    uint8_t icon_pixels_bg[24][24] = {0};

    std::string name = "";
    CircuitPressure ports[4];
    SimPoint current_simpoint;
    TestExecType monitor_state = MONITOR_STATE_PLAY_ALL;
    WrappedTexture* texture = NULL;

    int level_index;
    bool hidden = false;
    Circuit* circuit;
    LevelSet *best_design = NULL;
    LevelSet *saved_designs[4] = {NULL,NULL,NULL,NULL};

    int pin_order[4] = {-1, -1, -1, -1};

    unsigned connection_mask = 0;
    
    unsigned substep_count = 10000;
    
    std::vector<Test> tests;
    
    
    Pressure best_score = 0;
    Pressure last_score = 0;
    
    unsigned best_price = 0;
    unsigned last_price = 0;

    unsigned best_steam = 0;
    unsigned last_steam = 0;

    unsigned level_version = 0;
    
    
    bool touched = false;
    bool score_set = false;
    bool best_score_set = false;
    bool best_price_set = false;
    bool best_steam_set = false;
    bool server_refreshed = false;

    unsigned test_index = 0;
    unsigned sim_point_index = 0;
    unsigned substep_index = 0;

    class PressureRecord
    {
    public:
        Pressure values[4] = {-1, -1, -1, -1};
        unsigned marker = 0;
    } test_pressure_histroy[192];
    int test_pressure_histroy_index = 0;
    int test_pressure_histroy_sample_counter = 0;
    unsigned test_pressure_histroy_speed = 50;

    class FriendScore
    {
    public:
        std::string steam_username;
        uint64_t steam_id;
        Pressure score;
    };
    std::vector<FriendScore> friend_scores;


    int64_t global_score_graph[200] = {0};
    int64_t global_fetched_score;
    bool global_score_graph_set = false;
    unsigned global_score_graph_time = 0;

    Level(int level_index_, SaveObject* sobj);
    Level(int level_index_, bool hidden_ = false);
    ~Level();
    SaveObject* save(bool lite = false);

    XYPos getimage(DirFlip dir_flip);
    XYPos getimage_fg(DirFlip dir_flip);
    WrappedTexture* getimage_fg_texture();
    void setimage_fg_texture(WrappedTexture* texture_);

    void init_tests(SaveObjectMap* omap = NULL);
    void reset();
    void advance(unsigned ticks);
    void select_test(unsigned t);

    void update_score(bool fin);
    void set_monitor_state(TestExecType monitor_state_);
    void touch();
    void set_best_design(LevelSet* best);
};


class LevelSet
{
public:
    std::vector<Level*> levels;
    bool read_only = false;
    LevelSet(SaveObject* sobj, bool inspect = false);
    LevelSet();
    ~LevelSet();
    SaveObject* save_all(int level_index, bool lite = false);
    SaveObject* save_one(int level_index);
    bool is_playable(unsigned level, unsigned highest_level);
    int top_playable(int highest_level);
    Pressure test_level(int level_index);
    void record_best_score(int level_index);
    void save_design(int level_index, unsigned save_slot);
    void reset(int level_index);
    void remove_circles(int level_index);
    void touch(int level_index);
    unsigned new_user_level();
    unsigned import_level(LevelSet* other_set, int level_index);
    int find_custom_by_name(std::string name);
    void delete_level(int level_index);
    int find_level(int level_index, std::string name);
};
