#pragma once
#include "Misc.h"
#include "SaveState.h"
#include "Circuit.h"
#include <stdlib.h>

#define LEVEL_COUNT 9


class IOValue
{
public:
    Pressure out_value = 0;
    Pressure out_drive = 0;
    Pressure in_value = 0;
    Pressure recorded = 0;
    bool observed = false;

    IOValue(){}
    IOValue(Pressure out_value_):
        out_value(out_value_),
        out_drive(100),
        in_value(out_value_)
    {}

    IOValue(Pressure out_value_, Pressure out_drive_):
        out_value(out_value_),
        out_drive(out_drive_),
        in_value(out_value_)
    {}

    IOValue(Pressure out_value_, Pressure out_drive_, Pressure in_value_):
        out_value(out_value_),
        out_drive(out_drive_),
        in_value(in_value_)
    {}
    void set_recorded(Pressure p)
    {
        recorded = p;
        observed = true;
    }
    unsigned get_error()
    {
        if (in_value < 0)
            return 0;
        if (!observed)
            return 100;
        return abs(recorded - in_value);
    }

};

class SimPoint
{
public:
    IOValue N;
    IOValue E;
    IOValue S;
    IOValue W;

    SimPoint (IOValue N_, IOValue E_, IOValue S_, IOValue W_):
        N(N_),E(E_),S(S_), W(W_)
    {}
    SimPoint ()
    {}
    
    void record(Pressure pN, Pressure pE, Pressure pS, Pressure pW)
    {
        N.set_recorded(pressure_as_percent(pN));
        E.set_recorded(pressure_as_percent(pE));
        S.set_recorded(pressure_as_percent(pS));
        W.set_recorded(pressure_as_percent(pW));
    }

    IOValue& get(unsigned i)
    {
        switch (i)
        {
        case 0:
            return N;
        case 1:
            return E;
        case 2:
            return S;
        case 3:
            return W;
        }
        assert(0);
    }

};


class Level
{
public:
    CircuitPressure N;
    CircuitPressure E;
    CircuitPressure S;
    CircuitPressure W;

    unsigned level_index;
    Circuit* circuit;
    unsigned connection_mask = 0;

    
    unsigned substep_count = 2000;
    unsigned sim_point_count = 0;
    std::vector<SimPoint> sim_points;
    unsigned score_base = 100;
    
    unsigned best_score = 0;
    unsigned last_score = 0;
    bool enabled = false;
    
    unsigned sim_point_index;
    unsigned substep_index;
    
    Level(unsigned level_index_, SaveObject* sobj);
    Level(unsigned level_index_);
    SaveObject* save();

    void add_sim_point(SimPoint point, unsigned count);
    XYPos getimage(Direction direction);
    XYPos getimage_fg(Direction direction);

    void init();
    void reset(LevelSet* level_set);
    void advance(unsigned ticks);
    void enable_levels();

    unsigned get_score();

};


class LevelSet
{
public:
    Level* levels[LEVEL_COUNT];
    LevelSet(SaveObject* sobj);
    LevelSet();
    SaveObject* save();

};
