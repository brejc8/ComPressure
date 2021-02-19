#include "Level.h"

Test::Test()
{
        for (int i = 0; i < HISTORY_POINT_COUNT; i++)
        {
            last_pressure_log[i] = 0;
            best_pressure_log[i] = 0;
        }
        last_pressure_index = 0;

}

void Test::load(SaveObject* sobj)
{
    if (!sobj)
        return;
    SaveObjectMap* omap = sobj->get_map();
    last_score = omap->get_num("last_score");
    best_score = omap->get_num("best_score");

    {
        SaveObjectList* slist = omap->get_item("best_pressure_log")->get_list();
        for (int i = 0; i < HISTORY_POINT_COUNT; i++)
            best_pressure_log[i] = slist->get_num(i);
    }

    {
        SaveObjectList* slist = omap->get_item("last_pressure_log")->get_list();
        for (int i = 0; i < HISTORY_POINT_COUNT; i++)
            last_pressure_log[i] = slist->get_num(i);
    }
    last_pressure_index = omap->get_num("last_pressure_index");
}

SaveObject* Test::save()
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_num("last_score", last_score);
    omap->add_num("best_score", best_score);

    {
        SaveObjectList* slist = new SaveObjectList;
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                slist->add_num(best_pressure_log[i]);
        omap->add_item("best_pressure_log", slist);
    }

    {
        SaveObjectList* slist = new SaveObjectList;
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                slist->add_num(last_pressure_log[i]);
        omap->add_item("last_pressure_log", slist);
    }
        
    omap->add_num("last_pressure_index", last_pressure_index);

    
    return omap;
}

Level::Level(unsigned level_index_):
    level_index(level_index_)
{
    pin_order[0] = 0; pin_order[1] = 1; pin_order[2] = 2; pin_order[3] = 3;
    circuit = new Circuit;
    init_tests();
}

Level::Level(unsigned level_index_, SaveObject* sobj):
    level_index(level_index_)
{
    pin_order[0] = 0; pin_order[1] = 1; pin_order[2] = 2; pin_order[3] = 3;
    if (sobj)
    {
        SaveObjectMap* omap = sobj->get_map();
        circuit = new Circuit(omap->get_item("circuit")->get_map());
        int level_version = omap->get_num("level_version");

        init_tests(omap);
    }
    else
    {
        circuit = new Circuit;
        init_tests();
    }
}

Level::~Level()
{
    delete circuit;
}

SaveObject* Level::save(bool lite)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_item("circuit", circuit->save());
    omap->add_num("level_version", level_version);
    omap->add_num("best_score", best_score);

    if (!lite)
    {
        SaveObjectList* slist = new SaveObjectList;
        unsigned test_count = tests.size();
        for (unsigned i = 0; i < test_count; i++)
            slist->add_item(tests[i].save());
        omap->add_item("tests", slist);
    }
    return omap;
}

XYPos Level::getimage(Direction direction)
{
    int mask = connection_mask << direction | connection_mask >> (4 - direction) & 0xF;
    return XYPos(128 + (mask & 0x3) * 32, 32 + ((mask >> 2) & 0x3) * 32);
}

XYPos Level::getimage_fg(Direction direction)
{
    return XYPos(direction * 24, 184 + (int(level_index) * 24));
}


void Level::init_tests(SaveObjectMap* omap)
{
#define CONMASK_N          (1)
#define CONMASK_E          (2)
#define CONMASK_S          (4)
#define CONMASK_W          (8)

#define NEW_TEST do {tests.push_back({}); if (loaded_level_version == level_version && slist && slist->get_count() > tests.size()-1) tests.back().load(slist->get_item(tests.size()-1));} while (false)
#define NEW_POINT(a, b, c, d) tests.back().sim_points.push_back(SimPoint(a, b, c, d))
#define NEW_POINT_F(a, b, c, d, fa, fb, fc, fd) tests.back().sim_points.push_back(SimPoint(a, b, c, d, fa, fb, fc, fd))

    SaveObjectList* slist = NULL;
    if (omap && omap->get_item("tests"))
        omap->get_item("tests")->get_list();

    unsigned loaded_level_version = omap ? omap->get_num("level_version") : 0;

    switch(level_index)
    {
        case 0:
            connection_mask = CONMASK_N | CONMASK_W | CONMASK_E | CONMASK_S;            // Cross
            pin_order[0] = 3; pin_order[1] = 0; pin_order[2] = 1; pin_order[3] = 2;
            substep_count = 2000;
            level_version = 10;

            NEW_TEST; tests.back().tested_direction = DIRECTION_S;
            NEW_POINT(0  ,  0,  0,  0);
            NEW_POINT(100,  0,100,  0);
            NEW_TEST; tests.back().tested_direction = DIRECTION_S;
            NEW_POINT(100,  0,100,  0);
            NEW_POINT( 50,  0, 50,  0);
            NEW_TEST; tests.back().tested_direction = DIRECTION_S;
            NEW_POINT( 50,  0, 50,  0);
            NEW_POINT(  0,  0,  0,  0);

            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,  0,  0,  0);
            break;

        case 1:                                                                     // 50
            connection_mask = CONMASK_E;
            substep_count = 2000;
            level_version = 10;
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            break;

        case 2:                                                                     // NPN
            connection_mask = CONMASK_N | CONMASK_W | CONMASK_E;
            substep_count = 2000;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(100,  0,  0,  0);
            NEW_POINT(100, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(100, 50,  0, 50);
            NEW_POINT(100,100,  0,100);
            NEW_TEST;
            NEW_POINT(100,100,  0,100);
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0,100,  0, 50);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 50);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(100, 50,  0, 50);
            break;

        case 3:                                                                     // PNP
            connection_mask = CONMASK_S | CONMASK_W | CONMASK_E;
            substep_count = 2000;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0,100,100,100);
            NEW_POINT(  0,100,100, 50);
            NEW_TEST;
            NEW_POINT(  0,100,100, 50);
            NEW_POINT(  0,100,100,  0);
            NEW_TEST;
            NEW_POINT(  0,100,100,  0);
            NEW_POINT(  0, 50,  0, 50);
            break;

        case 4:                                                                     // select
            connection_mask = CONMASK_N | CONMASK_W | CONMASK_E | CONMASK_S;
            substep_count = 3000;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,100,  0);
            NEW_TEST;
            NEW_POINT(  0,100,100,  0);
            NEW_POINT(  0, 50, 50,  0);
            NEW_TEST;
            NEW_POINT(  0, 50, 50,  0);
            NEW_POINT(  0,  0, 50,100);
            NEW_TEST;
            NEW_POINT(  0,  0, 50,100);
            NEW_POINT(100,100, 50,100);
            NEW_TEST;
            NEW_POINT(100,100,  0,100);
            NEW_POINT(100, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(100, 50,  0, 50);
            NEW_POINT(  0, 50,100, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,100, 50);
            NEW_POINT( 20, 40, 60, 50);
            NEW_TEST;
            NEW_POINT( 20, 40, 60, 50);
            NEW_POINT(100,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(100,  0,  0,  0);
            NEW_POINT(100,100,  0,100);
            NEW_TEST;
            NEW_POINT(100,100,  0,100);
            NEW_POINT( 40, 40,  0,100);
            NEW_TEST;
            NEW_POINT( 40, 40,  0,100);
            NEW_POINT(  0,  0,  0,  0);
            break;


        case 5:                                                                 // Buf
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            circuit->force_element(XYPos(4,0), new CircuitElementEmpty());
            circuit->force_element(XYPos(4,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(4,2), new CircuitElementEmpty());
            circuit->force_element(XYPos(5,3), new CircuitElementValve(DIRECTION_E));
            circuit->force_element(XYPos(4,4), new CircuitElementEmpty());
            circuit->force_element(XYPos(5,5), new CircuitElementValve(DIRECTION_W));
            circuit->force_element(XYPos(4,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(4,7), new CircuitElementEmpty());
            circuit->force_element(XYPos(4,8), new CircuitElementEmpty());
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 10,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 10,  0, 10);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 90,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 90);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST; tests.back().tested_direction = DIRECTION_W;
            NEW_POINT(  0, 50,  0,100);
            NEW_POINT(  0, 50,  0,100);
            NEW_TEST; tests.back().tested_direction = DIRECTION_W;
            NEW_POINT(  0,  0,  0,100);
            NEW_POINT(  0,  0,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0, 90,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 90);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 20,  0, 20);
            NEW_TEST;
            NEW_POINT(  0, 20,  0, 20);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST; tests.back().tested_direction = DIRECTION_W;
            NEW_POINT(  0, 50,  0,  0);
            NEW_POINT(  0, 50,  0,  0);
            NEW_TEST; tests.back().tested_direction = DIRECTION_W;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0,100,  0,  0);
            break;


        case 6:                                                                     // Inv
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0, 90,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 10);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 40,  0, 60);
            NEW_TEST;
            NEW_POINT(  0, 40,  0, 60);
            NEW_POINT(  0, 10,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 10,  0, 90);
            NEW_POINT(  0,  0,  0,100);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,100);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0,  0,  0,100);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,100);
            NEW_POINT(  0,100,  0,  0);
            break;

        case 7:                                                             // op amp
            connection_mask = CONMASK_N | CONMASK_S | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,100,  0);
            NEW_POINT(  0,  0,100,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,100,  0);
            NEW_POINT(100,100,  0,  0);
            NEW_TEST;
            NEW_POINT(100,100,  0,  0);
            NEW_POINT( 10,  0, 90,  0);
            NEW_TEST;
            NEW_POINT( 10,  0, 90,  0);
            NEW_POINT( 80,100, 20,  0);
            NEW_TEST;
            NEW_POINT( 80,100, 20,  0);
            NEW_POINT( 30,  0, 70,  0);
            NEW_TEST;
            NEW_POINT( 30,  0, 70,  0);
            NEW_POINT( 60,100, 40,  0);
            NEW_TEST;
            NEW_POINT( 60,100, 40,  0);
            NEW_POINT( 20,  0, 30,  0);
            NEW_TEST;
            NEW_POINT( 20,  0, 30,  0);
            NEW_POINT( 80,100, 70,  0);
            NEW_TEST;
            NEW_POINT( 80,100, 70,  0);
            NEW_POINT( 40,  0, 50,  0);
            NEW_TEST;
            NEW_POINT( 40,  0, 50,  0);
            NEW_POINT( 55,100, 50,  0);
            NEW_TEST;
            NEW_POINT( 55,100, 50,  0);
            NEW_POINT( 55,  0, 60,  0);
            NEW_TEST;
            NEW_POINT( 55,  0, 60,  0);
            NEW_POINT(100,100, 95,  0);
            break;

        case 8:                                                                 // amp
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0,  0,  0, 10);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 10);
            NEW_POINT(  0,100,  0, 90);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 90);
            NEW_POINT(  0,  0,  0, 20);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 20);
            NEW_POINT(  0,100,  0, 80);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 80);
            NEW_POINT(  0,  0,  0, 30);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 30);
            NEW_POINT(  0,100,  0, 70);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 70);
            NEW_POINT(  0,  0,  0, 40);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 40);
            NEW_POINT(  0,100,  0, 60);
            break;

        case 9:                                                        // div 2
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT_F(  0,  0,  0,  0, 50, 50, 50, 50);
            NEW_POINT_F(  0,  0,  0,  0, 50, 50, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0,  0,  0,  0, 50, 50, 50, 80);
            NEW_POINT_F(  0, 50,  0,100, 50, 50, 50, 80);
            NEW_TEST;
            NEW_POINT_F(  0, 50,  0,100, 50, 50, 50, 40);
            NEW_POINT_F(  0, 30,  0, 60, 50, 50, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0, 30,  0, 60, 50, 50, 50,100);
            NEW_POINT_F(  0, 25,  0, 50, 50, 50, 50,100);
            NEW_TEST;
            NEW_POINT_F(  0, 25,  0, 50, 50, 50, 50, 60);
            NEW_POINT_F(  0, 20,  0, 40, 50, 50, 50, 60);
            NEW_TEST;
            NEW_POINT_F(  0, 20,  0, 40, 50, 50, 50, 80);
            NEW_POINT_F(  0, 10,  0, 20, 50, 50, 50, 80);
            NEW_TEST;
            NEW_POINT_F(  0, 10,  0, 20, 50, 50, 50, 40);
            NEW_POINT_F(  0, 25,  0, 50, 50, 50, 50, 40);
            NEW_TEST;
            NEW_POINT_F(  0, 25,  0, 50, 50, 50, 50, 60);
            NEW_POINT_F(  0, 50,  0,100, 50, 50, 50, 60);
            NEW_TEST;
            NEW_POINT_F(  0, 50,  0,100, 50, 50, 50, 40);
            NEW_POINT_F(  0,  0,  0,  0, 50, 50, 50, 40);
            NEW_TEST;
            NEW_POINT_F(  0,  0,  0,  0, 50, 50, 50, 50);
            NEW_POINT_F(  0, 50,  0,100, 50, 50, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0, 50,  0,100, 50, 50, 50, 40);
            NEW_POINT_F(  0,  0,  0,  0, 50, 50, 50, 40);
            NEW_TEST;
            NEW_POINT_F(  0,  0,  0,  0, 50, 50, 50, 50);
            NEW_POINT_F(  0, 50,  0,100, 50, 50, 50, 50);
            break;

        case 10:                                                        // mul 2
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,  0, 50);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 50);
            NEW_POINT(  0, 80,  0, 40);
            NEW_TEST;
            NEW_POINT(  0, 80,  0, 40);
            NEW_POINT(  0, 60,  0, 30);
            NEW_TEST;
            NEW_POINT(  0, 60,  0, 30);
            NEW_POINT(  0, 40,  0, 20);
            NEW_TEST;
            NEW_POINT(  0, 40,  0, 20);
            NEW_POINT(  0, 20,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 20,  0, 10);
            NEW_POINT(  0, 50,  0, 25);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 25);
            NEW_POINT(  0,  0,  0,  0);
            break;

        case 11:                                                        // Encrypt
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 11;
            
            circuit->force_element(XYPos(0,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(2,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(0,2), new CircuitElementPipe(CONNECTIONS_ES));
            circuit->force_element(XYPos(1,2), new CircuitElementValve(DIRECTION_E));
            circuit->force_element(XYPos(2,2), new CircuitElementSource(DIRECTION_W));
            circuit->force_element(XYPos(0,3), new CircuitElementPipe(CONNECTIONS_NES));
            circuit->force_element(XYPos(1,3), new CircuitElementValve(DIRECTION_E));
            circuit->force_element(XYPos(2,3), new CircuitElementSource(DIRECTION_W));
            circuit->force_element(XYPos(0,4), new CircuitElementPipe(CONNECTIONS_NWS));
            circuit->force_element(XYPos(1,4), new CircuitElementPipe(CONNECTIONS_NES));
            circuit->force_element(XYPos(2,4), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(0,5), new CircuitElementPipe(CONNECTIONS_NE));
            circuit->force_element(XYPos(1,5), new CircuitElementValve(DIRECTION_W));
            circuit->force_element(XYPos(2,5), new CircuitElementEmpty());
            circuit->force_element(XYPos(0,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,6), new CircuitElementSource(DIRECTION_N));
            circuit->force_element(XYPos(2,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(0,7), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,7), new CircuitElementEmpty());
            circuit->force_element(XYPos(2,7), new CircuitElementEmpty());

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 24,  0, 13);
            NEW_TEST;
            NEW_POINT(  0, 24,  0, 13);
            NEW_POINT(  0, 27,  0, 15);
            NEW_TEST;
            NEW_POINT(  0, 27,  0, 15);
            NEW_POINT(  0, 64,  0, 48);
            NEW_TEST;
            NEW_POINT(  0, 64,  0, 48);
            NEW_POINT(  0, 69,  0, 54);
            NEW_TEST;
            NEW_POINT(  0, 69,  0, 54);
            NEW_POINT(  0, 83,  0, 73);
            NEW_TEST;
            NEW_POINT(  0, 83,  0, 73);
            NEW_POINT(  0, 85,  0, 76);
            NEW_TEST;
            NEW_POINT(  0, 85,  0, 76);
            NEW_POINT(  0, 87,  0, 79);
            NEW_TEST;
            NEW_POINT(  0, 87,  0, 79);
            NEW_POINT(  0, 94,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 94,  0, 90);
            NEW_POINT(  0, 97,  0, 95);
            NEW_TEST;
            NEW_POINT(  0, 97,  0, 95);
            NEW_POINT(  0,100,  0,100);
            break;


        case 12:                                                        // Decrypt
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 11;
            substep_count = 20000;
            circuit->force_element(XYPos(3,4), new CircuitElementSubCircuit(DIRECTION_N, 7));

            circuit->force_element(XYPos(4,5), new CircuitElementSubCircuit(DIRECTION_S, 11));

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 13,  0, 24);
            NEW_TEST;
            NEW_POINT(  0, 13,  0, 24);
            NEW_POINT(  0, 15,  0, 27);
            NEW_TEST;
            NEW_POINT(  0, 15,  0, 27);
            NEW_POINT(  0, 48,  0, 64);
            NEW_TEST;
            NEW_POINT(  0, 48,  0, 64);
            NEW_POINT(  0, 54,  0, 69);
            NEW_TEST;
            NEW_POINT(  0, 54,  0, 69);
            NEW_POINT(  0, 73,  0, 83);
            NEW_TEST;
            NEW_POINT(  0, 73,  0, 83);
            NEW_POINT(  0, 76,  0, 85);
            NEW_TEST;
            NEW_POINT(  0, 76,  0, 85);
            NEW_POINT(  0, 79,  0, 87);
            NEW_TEST;
            NEW_POINT(  0, 79,  0, 87);
            NEW_POINT(  0, 90,  0, 94);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 94);
            NEW_POINT(  0, 95,  0, 97);


            break;


        case 13:                                                    // mean
            substep_count = 20000;
            connection_mask = CONMASK_N | CONMASK_S | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT_F(  0,  0,  0,  0, 10, 50, 50, 50);
            NEW_POINT_F(  0,  0,  0,  0, 10, 50, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0,  0,  0,  0, 10, 50, 50, 50);
            NEW_POINT_F( 50, 50, 50,  0, 80, 50, 10, 50);
            NEW_TEST;
            NEW_POINT_F( 50, 50, 50,  0, 80, 50, 10, 50);
            NEW_POINT_F(100,100,100,  0, 60, 50, 10, 50);
            NEW_TEST;
            NEW_POINT_F(100,100,100,  0, 60, 50, 10, 50);
            NEW_POINT_F(  0, 50,100,  0, 20, 50, 70, 50);
            NEW_TEST;
            NEW_POINT_F(  0, 50,100,  0, 20, 50, 70, 50);
            NEW_POINT_F(100, 50,  0,  0, 60, 50, 20, 50);
            NEW_TEST;
            NEW_POINT_F(100, 50,  0,  0, 60, 50, 20, 50);
            NEW_POINT_F( 40, 20,  0,  0, 20, 50, 80, 50);
            NEW_TEST;
            NEW_POINT_F( 40, 20,  0,  0, 20, 50, 80, 50);
            NEW_POINT_F(  0, 30, 60,  0, 70, 50, 20, 50);
            NEW_TEST;
            NEW_POINT_F(  0, 30, 60,  0, 70, 50, 20, 50);
            NEW_POINT_F(100, 80, 60,  0, 70, 50, 40, 50);
            NEW_TEST;
            NEW_POINT_F(100, 80, 60,  0, 70, 50, 40, 50);
            NEW_POINT_F(50,  55, 60,  0, 90, 50, 50, 50);
            NEW_TEST;
            NEW_POINT_F(50,  55, 60,  0, 90, 50, 50, 50);
            NEW_POINT_F(10,  5,   0,  0, 70, 50, 30, 50);
            break;

        case 14:                                                    // add
            substep_count = 20000;
            connection_mask = CONMASK_N | CONMASK_S | CONMASK_E;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT( 20, 20,  0,  0);
            NEW_TEST;
            NEW_POINT( 20, 20,  0,  0);
            NEW_POINT( 70, 80, 10,  0);
            NEW_TEST;
            NEW_POINT( 70, 80, 10,  0);
            NEW_POINT( 40, 80, 40,  0);
            NEW_TEST;
            NEW_POINT( 40, 80, 40,  0);
            NEW_POINT( 50, 70, 20,  0);
            NEW_TEST;
            NEW_POINT( 50, 70, 20,  0);
            NEW_POINT( 70, 80, 10,  0);
            NEW_TEST;
            NEW_POINT( 70, 80, 10,  0);
            NEW_POINT( 25, 75, 50,  0);
            NEW_TEST;
            NEW_POINT( 25, 75, 50,  0);
            NEW_POINT(  0, 50, 50,  0);
            NEW_TEST;
            NEW_POINT(  0, 50, 50,  0);
            NEW_POINT( 10, 80, 70,  0);
            NEW_TEST;
            NEW_POINT( 10, 80, 70,  0);
            NEW_POINT( 80, 90, 10,  0);
            NEW_TEST;
            NEW_POINT( 80, 90, 10,  0);
            NEW_POINT( 20, 40, 20,  0);
            NEW_TEST;
            NEW_POINT( 20, 40, 20,  0);
            NEW_POINT( 60,100, 40,  0);
            NEW_TEST;
            NEW_POINT( 60,100, 40,  0);
            NEW_POINT( 20, 80, 60,  0);
            NEW_TEST;
            NEW_POINT( 20, 80, 60,  0);
            NEW_POINT(  0,  0,  0,  0);
            break;

        case 15:                                                    // subtract
            substep_count = 30000;
            connection_mask = CONMASK_W | CONMASK_S | CONMASK_E;
            pin_order[0] = 3; pin_order[1] = 2; pin_order[2] = 1; pin_order[3] = 0;
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 40, 50, 90);
            NEW_TEST;
            NEW_POINT(  0, 40, 50, 90);
            NEW_POINT(  0, 50, 20, 70);
            NEW_TEST;
            NEW_POINT(  0, 50, 20, 70);
            NEW_POINT(  0,  0, 70, 70);
            NEW_TEST;
            NEW_POINT(  0,  0, 70, 70);
            NEW_POINT(  0, 25, 25, 50);
            NEW_TEST;
            NEW_POINT(  0, 25, 25, 50);
            NEW_POINT(  0, 50, 20, 70);
            NEW_TEST;
            NEW_POINT(  0, 50, 20, 70);
            NEW_POINT(  0, 20, 20, 40);
            NEW_TEST;
            NEW_POINT(  0, 20, 20, 40);
            NEW_POINT(  0, 40,  0, 40);
            NEW_TEST;
            NEW_POINT(  0, 40,  0, 40);
            NEW_POINT(  0,  0, 40, 40);
            NEW_TEST;
            NEW_POINT(  0,  0, 40, 40);
            NEW_POINT(  0, 50, 50,100);
            break;

 
        case 16:                                                    // end
            connection_mask = CONMASK_W | CONMASK_S | CONMASK_E;
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0, 77,  0,  0);
            break;
 
//         case 10:                                                             // amp inv
//             connection_mask = CONMASK_W | CONMASK_E;
// 
//             NEW_TEST;// N   E   S   W
//             NEW_POINT(  0,  0,  0,  0);
//             NEW_POINT(  0,  0,  0,100);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0,100);
//             NEW_POINT(  0,100,  0, 10);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 10);
//             NEW_POINT(  0,  0,  0, 90);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0, 90);
//             NEW_POINT(  0,100,  0, 20);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 20);
//             NEW_POINT(  0,  0,  0, 80);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0, 80);
//             NEW_POINT(  0,100,  0, 30);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 30);
//             NEW_POINT(  0,  0,  0, 70);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0, 70);
//             NEW_POINT(  0,100,  0, 40);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 40);
//             NEW_POINT(  0,  0,  0, 60);
//             break;
// 
// 
// 
//         case 12:                                                    // zoom amplify
//             substep_count = 30000;
//             connection_mask = CONMASK_N | CONMASK_W | CONMASK_E | CONMASK_S;
// 
//             NEW_TEST;// N   E   S   W
//             NEW_POINT(100,  0,  0,  0);
//             NEW_POINT(100,  0,  0,  0);
//             NEW_TEST;
//             NEW_POINT(100,  0,  0,  0);
//             NEW_POINT(100,100,  0,100);
//             NEW_TEST;
//             NEW_POINT(100,100,  0,100);
//             NEW_POINT(100,  0, 50, 50);
//             NEW_TEST;
//             NEW_POINT(100,  0, 50, 50);
//             NEW_POINT( 90,100, 50, 80);
//             NEW_TEST;
//             NEW_POINT( 90,100, 50, 80);
//             NEW_POINT( 90,  0, 50, 60);
//             NEW_TEST;
//             NEW_POINT( 90,  0, 50, 60);
//             NEW_POINT( 80,100, 20, 60);
//             NEW_TEST;
//             NEW_POINT( 80,100, 20, 60);
//             NEW_POINT( 80,  0, 20, 40);
//             NEW_TEST;
//             NEW_POINT( 80,  0, 20, 40);
//             NEW_POINT( 80,100, 30, 60);
//             NEW_TEST;
//             NEW_POINT( 80,100, 30, 60);
//             NEW_POINT(100,  0,  0, 45);
//             NEW_TEST;
//             NEW_POINT(100,  0,  0, 45);
//             NEW_POINT(100,100,  0, 55);
//             NEW_TEST;
//             NEW_POINT(100,100,  0, 55);
//             NEW_POINT( 60,100, 45, 55);
//             NEW_TEST;
//             NEW_POINT( 60,100, 45, 55);
//             NEW_POINT( 60,  0, 45, 50);
//             break;
// 
        default:
            printf("no level %d\n", level_index);
            break;
    }

    if (loaded_level_version == level_version)
        best_score = omap ? omap->get_num("best_score") : 0;
}

void Level::reset(LevelSet* level_set)
{
    test_index = 0;
    sim_point_index = 0;
    substep_index = 0;
    in_range_count = 0;
    touched = false;

    circuit->elaborate(level_set);
    circuit->reset();
    for (int i = 0; i < 4; i++)
        ports[i] = 0;
}

void Level::advance(unsigned ticks, TestExecType type)
{
    static int val = 0;
    for (int tick = 0; tick < ticks; tick++)
    {
        SimPoint& simp = tests[test_index].sim_points[sim_point_index];

        for (int p = 0; p < 4; p++)
            ports[p].pre();

        circuit->sim_pre(PressureAdjacent(ports[0], ports[1], ports[2], ports[3]));
        for (int p = 0; p < 4; p++)
        {
            if (((connection_mask >> p) & 1) && p != tests[test_index].tested_direction)
                ports[p].apply(simp.values[p], simp.force[p]);
        }
        circuit->sim_post(PressureAdjacent(ports[0], ports[1], ports[2], ports[3]));

        for (int p = 0; p < 4; p++)
            ports[p].post();

        if (sim_point_index == tests[test_index].sim_points.size() - 1)
        {
            Pressure value = ports[tests[test_index].tested_direction].value;
            unsigned index = (substep_index * HISTORY_POINT_COUNT) / substep_count;
            tests[test_index].last_pressure_log[index] = value;
            tests[test_index].last_pressure_index = index + 1;
        }

        substep_index++;

        if (substep_index >= substep_count)
        {
            substep_index  = 0;
            if (sim_point_index == tests[test_index].sim_points.size() - 1)
            {
                {
                    Direction p = tests[test_index].tested_direction;
                    Pressure score = percent_as_pressure(25) - abs(percent_as_pressure(simp.values[p]) - ports[p].value) * 100;
                    if (score < 0)
                        score /= 2;
                    score += percent_as_pressure(25);
                    if (score < 0)
                        score /= 2;
                    score += percent_as_pressure(25);
                    if (score < 0)
                        score /= 2;
                    score += percent_as_pressure(25);
                    if (score < 0)
                        score = 0;
                    tests[test_index].last_score = score;
                    sim_point_index = 0;
                    substep_index = 0;
                    if (type == MONITOR_STATE_PLAY_ALL)
                    {
                        test_index++;
                        if (test_index == tests.size())
                        {
                            update_score();
                            test_index = 0;
                            circuit->reset();
                            touched = false;
                        }
                    }
                }
            }
            else
            {
                sim_point_index++;
            }
        }
    }
}

void Level::select_test(unsigned t)
{
    if (t >= tests.size())
        return;
    sim_point_index = 0;
    substep_index = 0;
    test_index = t;
}

void Level::update_score()
{
    unsigned test_count = tests.size();
    Pressure score = 0;
    for (unsigned i = 0; i < test_count; i++)
    {
        score += tests[i].last_score;
    }
    score /= test_count;
    last_score = score;
    if (score > best_score)
    {
        best_score = score;
        for (unsigned t = 0; t < test_count; t++)
        {
            tests[t].best_score = tests[t].last_score;
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                tests[t].best_pressure_log[i] = tests[t].last_pressure_log[i];
        }
        
        
        SaveObject* omap = circuit->save();
        omap->save(std::cout);
        
        
    }
    return;
}

LevelSet::LevelSet(SaveObject* sobj)
{
    SaveObjectList* slist = sobj->get_list();
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        if (i < slist->get_count())
            levels[i] = new Level(i, slist->get_item(i));
        else
            levels[i] = new Level(i);
    }
}

LevelSet::LevelSet()
{
        for (int i = 0; i < LEVEL_COUNT; i++)
        {
            levels[i] = new Level(i);
        }
}

LevelSet::~LevelSet()
{
        for (int i = 0; i < LEVEL_COUNT; i++)
        {
            delete levels[i];
        }
}

SaveObject* LevelSet::save(bool lite)
{
    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        slist->add_item(levels[i]->save(lite));
    }
    return slist;
}
bool LevelSet::is_playable(unsigned level)
{
    if (level >= LEVEL_COUNT)
        return false;
    for (int i = 0; i < level; i++)
    {
        if (levels[i]->best_score < percent_as_pressure(75))
            return false;
    }
    return true;
}
int LevelSet::top_playable()
{
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        if (levels[i]->best_score < percent_as_pressure(75))
            return i;
    }
    return LEVEL_COUNT - 1;
}
