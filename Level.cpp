#include "Level.h"

Level::Level(unsigned level_index_):
    level_index(level_index_),
    sim_points(128)
{
    circuit = new Circuit;
    init();
}

Level::Level(unsigned level_index_, SaveObject* sobj):
    level_index(level_index_),
    sim_points(128)
{
    if (sobj)
        circuit = new Circuit(sobj->get_map()->get_item("circuit")->get_map());
    else
        circuit = new Circuit;
    init();
}

SaveObject* Level::save()
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_item("circuit", circuit->save());
    return omap;
}

void Level::add_sim_point(SimPoint point, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        sim_points[sim_point_count] = point;

        sim_point_count++;
        assert(sim_point_count <  128);
    }
}

void Level::init(void)
{
#define DRIVE(a)   IOValue(a, 100, 0)
#define FLOAT(A)   IOValue(0, 0, A)

    switch(level_index)
    {
        case 0:              //       N            E             S            W                  time
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   DRIVE(100)  ,FLOAT(0)     ,FLOAT(100)  ,FLOAT(0)    ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   FLOAT(100)  ,FLOAT(0)     ,DRIVE(100)  ,FLOAT(0)    ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   FLOAT(0)    ,DRIVE(100)   ,FLOAT(0)    ,FLOAT(100)  ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   FLOAT(0)    ,FLOAT(100)   ,FLOAT(0)    ,DRIVE(100)  ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);

            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   DRIVE(50)   ,FLOAT(0)     ,FLOAT(50)   ,FLOAT(0)    ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   FLOAT(50)   ,FLOAT(0)     ,DRIVE(50)   ,FLOAT(0)    ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   FLOAT(0)    ,DRIVE(50)    ,FLOAT(0)    ,FLOAT(50)   ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);
            add_sim_point(SimPoint(   FLOAT(0)    ,FLOAT(50)    ,FLOAT(0)    ,DRIVE(50)   ),     1);
            add_sim_point(SimPoint(   DRIVE(0)    ,DRIVE(0)     ,DRIVE(0)    ,DRIVE(0)    ),     1);

            add_sim_point(SimPoint(   DRIVE(100)  ,DRIVE(100)   ,DRIVE(100)  ,DRIVE(100)  ),     1);
            add_sim_point(SimPoint(   DRIVE(50)   ,FLOAT(100)   ,FLOAT(50)   ,FLOAT(100)  ),     1);
            add_sim_point(SimPoint(   DRIVE(100)  ,DRIVE(100)   ,DRIVE(100)  ,DRIVE(100)  ),     1);
            add_sim_point(SimPoint(   FLOAT(50)   ,FLOAT(100)   ,DRIVE(50)   ,FLOAT(100)  ),     1);
            add_sim_point(SimPoint(   DRIVE(100)  ,DRIVE(100)   ,DRIVE(100)  ,DRIVE(100)  ),     1);
            add_sim_point(SimPoint(   FLOAT(100)  ,DRIVE(50)    ,FLOAT(100)  ,FLOAT(50)   ),     1);
            add_sim_point(SimPoint(   DRIVE(100)  ,DRIVE(100)   ,DRIVE(100)  ,DRIVE(100)  ),     1);
            add_sim_point(SimPoint(   FLOAT(100)  ,FLOAT(50)    ,FLOAT(100)  ,DRIVE(50)   ),     1);
            add_sim_point(SimPoint(   DRIVE(100)  ,DRIVE(100)   ,DRIVE(100)  ,DRIVE(100)  ),     1);
            break;
        default:
            printf("no level %d\n", level_index);
            break;
    }
}

void Level::reset()
{
    sim_point_index = 0;
    substep_index = 0;

    circuit->reset();
    N=0;
    S=0;
    E=0;
    W=0;
}

void Level::advance(unsigned ticks)
{
    static int val = 0;
    for (int i = 0; i < ticks; i++)
    {
        SimPoint& simp = sim_points[sim_point_index];
        N.apply(simp.N.out_value, simp.N.out_drive);
        E.apply(simp.E.out_value, simp.E.out_drive);
        S.apply(simp.S.out_value, simp.S.out_drive);
        W.apply(simp.W.out_value, simp.W.out_drive);
        
        N.pre();
        E.pre();
        S.pre();
        W.pre();

        circuit->sim_pre(PressureAdjacent(N, E, S, W));
        circuit->sim_post(PressureAdjacent(N, E, S, W));

        N.post();
        E.post();
        S.post();
        W.post();
        
        substep_index++;
        if (substep_index >= 1000)
        {
            substep_index = 0;
            simp.record(N.value, E.value, S.value, W.value);
//            printf("%d %d %d %d\n", pressure_as_percent(N.value), pressure_as_percent(E.value), pressure_as_percent(S.value), pressure_as_percent(W.value));
            
            sim_point_index++;
            if (sim_point_index >= sim_point_count)
                sim_point_index = 0;
        }
    }
}
