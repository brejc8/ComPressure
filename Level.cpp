#include "Level.h"
#include "SaveState.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <limits>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <codecvt>

static SaveObjectList* make_level_desc()
{
    std::string text;
    #include "Level.string"
    std::istringstream decomp_stream(text);

    return SaveObject::load(decomp_stream)->get_list();
}

SaveObjectList* level_desc = make_level_desc();

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

Level::Level(unsigned level_index_, bool hidden_):
    level_index(level_index_),
    hidden(hidden_)
{
    pin_order[0] = -1; pin_order[1] = -1; pin_order[2] = -1; pin_order[3] = -1;
    circuit = new Circuit;
    init_tests();
}

Level::Level(unsigned level_index_, SaveObject* sobj):
    level_index(level_index_)
{
    pin_order[0] = -1; pin_order[1] = -1; pin_order[2] = -1; pin_order[3] = -1;
    SaveObjectMap* omap = sobj->get_map();
    circuit = new Circuit(omap->get_item("circuit")->get_map());
    if (omap->has_key("best_design"))
        best_design = new LevelSet(omap->get_item("best_design"), true);
    if (omap->has_key("saved_designs"))
    {
        SaveObjectList* slist = omap->get_item("saved_designs")->get_list();
        for (unsigned i = 0; i < 4; i++)
        {
            if (i < slist->get_count())
            {
                SaveObject *sobj = slist->get_item(i);
                if (!sobj->is_null())
                    saved_designs[i] = new LevelSet(sobj, true);
            }
                
        }
    }

    init_tests(omap);
}

Level::~Level()
{
    delete circuit;
    delete best_design;
    for (unsigned i = 0; i < 4; i++)
        delete saved_designs[i];
}

SaveObject* Level::save(bool lite)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_item("circuit", circuit->save());

    omap->add_num("level_version", level_version);
    if (!lite)
    {
        omap->add_num("best_score", best_score);
        omap->add_num("last_score", last_score);
        omap->add_num("best_price", best_price);
        omap->add_num("last_price", last_price);
        omap->add_num("best_steam", best_steam);
        omap->add_num("last_steam", last_steam);
        SaveObjectList* slist = new SaveObjectList;
        unsigned test_count = tests.size();
        for (unsigned i = 0; i < test_count; i++)
            slist->add_item(tests[i].save());
        omap->add_item("tests", slist);
        if (best_design)
            omap->add_item("best_design", best_design->save_one(level_index));

        slist = new SaveObjectList;
        for (unsigned i = 0; i < 4; i++)
            if (saved_designs[i])
                slist->add_item(saved_designs[i]->save_one(level_index));
            else
                slist->add_item(new SaveObjectNull);
        omap->add_item("saved_designs", slist);
    }
    else
    {
        omap->add_num("best_score", score_set ? last_score : 0);
    }

    return omap;
}

XYPos Level::getimage(DirFlip dir_flip)
{
    int mask = dir_flip.mask(connection_mask);
    return XYPos(128 + (mask & 0x3) * 32, 32 + ((mask >> 2) & 0x3) * 32);
}

XYPos Level::getimage_fg(DirFlip dir_flip)
{
    return XYPos(dir_flip.as_int() * 24, 184 + (int(level_index) * 24));
}


void Level::init_tests(SaveObjectMap* omap)
{
    SaveObjectList* slist = NULL;
    if (omap && omap->has_key("tests"))
        slist = omap->get_item("tests")->get_list();

    unsigned loaded_level_version = 0;
    if (omap && omap->has_key("level_version"))
        loaded_level_version = omap->get_num("level_version");
    
    SaveObjectMap* desc = level_desc->get_item(level_index)->get_map();
    
    
    name = desc->get_string("name");
    
    SaveObjectList* conlist = desc->get_item("connections")->get_list();
    for (unsigned i = 0; i < conlist->get_count(); i++)
    {
        unsigned port_num = conlist->get_num(i);
        pin_order[i] = port_num;
        connection_mask |= 1 << port_num;
    }
    substep_count = desc->get_num("substep_count");
    level_version = desc->get_num("level_version");

    if (desc->has_key("forced_elements"))
    {
        SaveObjectList* forced_list = desc->get_item("forced_elements")->get_list();
        for (unsigned i = 0; i < forced_list->get_count(); i++)
        {
            SaveObjectMap* forced_map = forced_list->get_item(i)->get_map();
            XYPos pos(forced_map->get_num("x"), forced_map->get_num("y"));
            CircuitElement* elem = CircuitElement::load(forced_map->get_item("element"), true);
            circuit->force_element(pos, elem);
        }
    }
    
    if (desc->has_key("forced_signs"))
    {
        SaveObjectList* forced_list = desc->get_item("forced_signs")->get_list();
        for (unsigned i = 0; i < forced_list->get_count(); i++)
        {
            Sign new_sign(forced_list->get_item(i));
            circuit->force_sign(new_sign);
        }
    }
    

    SaveObjectList* testlist = desc->get_item("tests")->get_list();
    for (unsigned i = 0; i < testlist->get_count(); i++)
    {
        tests.push_back({});
        Test& t = tests.back();
        
        if (loaded_level_version == level_version && slist && slist->get_count() > tests.size() - 1)
            t.load(slist->get_item(tests.size() - 1));
        
        SaveObjectMap* test_map = testlist->get_item(i)->get_map();

        if (test_map->has_key("tested_direction"))
            t.tested_direction = Direction(test_map->get_num("tested_direction"));
        if (test_map->has_key("reset"))
        {
            if (test_map->get_num("reset") == 1)
                t.reset_1 = true;
            else
                t.reset_all = true;
        }

        SaveObjectList* pointlist = test_map->get_item("points")->get_list();
        for (unsigned j = 0; j < pointlist->get_count(); j++)
        {
            SimPoint point;
            SaveObjectMap* point_map = pointlist->get_item(j)->get_map();
            if (point_map->has_key("N")) point.values[0] = point_map->get_num("N");
            if (point_map->has_key("E")) point.values[1] = point_map->get_num("E");
            if (point_map->has_key("S")) point.values[2] = point_map->get_num("S");
            if (point_map->has_key("W")) point.values[3] = point_map->get_num("W");

            if (point_map->has_key("NF")) point.force[0] = point_map->get_num("NF");
            else point.force[0] = t.tested_direction == DIRECTION_N ? 0 : 50;
            if (point_map->has_key("EF")) point.force[1] = point_map->get_num("EF");
            else point.force[1] = t.tested_direction == DIRECTION_E ? 0 : 50;
            if (point_map->has_key("SF")) point.force[2] = point_map->get_num("SF");
            else point.force[2] = t.tested_direction == DIRECTION_S ? 0 : 50;
            if (point_map->has_key("WF")) point.force[3] = point_map->get_num("WF");
            else point.force[3] = t.tested_direction == DIRECTION_W ? 0 : 50;
            
            if (point_map->has_key("PRESET"))
                t.first_simpoint = j + 1;

            t.sim_points.push_back(point);
        }
    }

    if (loaded_level_version == level_version && omap)
    {
        if (omap->has_key("best_score"))
            best_score = omap->get_num("best_score");
        if (omap->has_key("last_score"))
            last_score = omap->get_num("last_score");
        if (omap->has_key("best_price"))
            best_price = omap->get_num("best_price");
        if (omap->has_key("last_price"))
            last_price = omap->get_num("last_price");
        if (omap->has_key("best_steam"))
            best_steam = omap->get_num("best_steam");
        if (omap->has_key("last_steam"))
            last_steam = omap->get_num("last_steam");
    }
}

void Level::reset()
{
    test_index = 0;
    sim_point_index = tests[test_index].first_simpoint;
    substep_index = 0;
    touched = false;
    current_simpoint = tests[test_index].sim_points[sim_point_index];

    circuit->reset();
    for (int i = 0; i < 4; i++)
        ports[i] = 0;
    last_price = circuit->get_cost();
    circuit->reset_steam_used();
}

void Level::advance(unsigned ticks)
{
    unsigned test_pressure_histroy_sample_interval = pow(1.05, test_pressure_histroy_speed) * 10;

    circuit->prep(PressureAdjacent(ports[0], ports[1], ports[2], ports[3]));

    static int val = 0;
    for (int tick = 0; tick < ticks; tick++)
    {
        for (int p = 0; p < 4; p++)
            ports[p].pre();

        for (int p = 0; p < 4; p++)
        {
            if ((((connection_mask >> p) & 1)) || (monitor_state == MONITOR_STATE_PAUSE))
                ports[p].apply(current_simpoint.values[p], current_simpoint.force[p]);
        }
        circuit->sim_pre(PressureAdjacent(ports[0], ports[1], ports[2], ports[3]));

        for (int p = 0; p < 4; p++)
            ports[p].post();

        if (sim_point_index == tests[test_index].sim_points.size() - 1)
        {
            Pressure value = ports[tests[test_index].tested_direction].value;
            unsigned index = (substep_index * HISTORY_POINT_COUNT) / substep_count;
            tests[test_index].last_pressure_log[index] = value;
            tests[test_index].last_pressure_index = index + 1;
        }

        if (monitor_state != MONITOR_STATE_PAUSE)
        {
            substep_index++;
            if (substep_index >= substep_count)
            {
                substep_index  = 0;
                if (sim_point_index == tests[test_index].sim_points.size() - 1)
                {
                    Direction p = tests[test_index].tested_direction;
                    Pressure score = percent_as_pressure(100) - abs(percent_as_pressure(current_simpoint.values[p]) - ports[p].value) * (100 / 5);
                    if (score < 0)
                        score = 0;
                    tests[test_index].last_score = score;
                    update_score(false);
                    sim_point_index = 0;
                    substep_index = 0;
                    if (monitor_state == MONITOR_STATE_PLAY_ALL)
                    {
                        test_index++;
                        if (test_index == tests.size())
                        {
                            if (!touched)
                                update_score(true);
                            reset();
                        }
                        sim_point_index = tests[test_index].first_simpoint;
                    }
                    if ((monitor_state == MONITOR_STATE_PLAY_1 && tests[test_index].reset_1) ||
                        (monitor_state == MONITOR_STATE_PLAY_ALL && tests[test_index].reset_all) || 
                        test_index == 0)
                    {
                        circuit->reset();
                        for (int i = 0; i < 4; i++)
                            ports[i] = 0;
                    }
                }
                else
                {
                    sim_point_index++;
                }
                current_simpoint = tests[test_index].sim_points[sim_point_index];
            }
        }

        if ((test_pressure_histroy_sample_counter % 10000) == 0)
        {
            test_pressure_histroy[test_pressure_histroy_index].marker = 1;
        
        }
        
        if ((test_pressure_histroy_sample_counter % test_pressure_histroy_sample_interval) == 0)
        {
            for (int p = 0; p < 4; p++)
                test_pressure_histroy[test_pressure_histroy_index].values[p] = ports[p].value;
            test_pressure_histroy_index = (test_pressure_histroy_index + 1) % 192;
            test_pressure_histroy[test_pressure_histroy_index].marker = 0;

        }
        test_pressure_histroy_sample_counter++;
    }
}

void Level::select_test(unsigned t)
{
    if (t >= tests.size())
        return;
    test_index = t;

    sim_point_index = 0;
    touched = true;
    substep_index = 0;
    set_monitor_state(MONITOR_STATE_PLAY_1);
    if (tests[test_index].reset_1 || test_index == 0)
    {
        circuit->reset();
        for (int i = 0; i < 4; i++)
            ports[i] = 0;
    }

}

void Level::update_score(bool fin)
{
    unsigned test_count = tests.size();
    Pressure score = percent_as_pressure(100);
    for (unsigned i = 0; i < test_count; i++)
    {
        if (tests[i].last_score < score)
            score = tests[i].last_score;
    }
    last_score = score;

    if (fin)
    {
        last_steam = circuit->get_steam_used();
        score_set = true;
    }
    if (!score)
        return;

    if ((last_price < best_price || !best_design) && fin)
    {
        best_price = last_price;
        best_price_set = true;
    }

    if ((last_steam < best_steam || !best_design) && fin)
    {
        best_steam = last_steam;
        best_steam_set = true;
    }

    if ((last_score > best_score || !best_design) && fin)
    {
        best_score = last_score;
        best_score_set = true;
    }

   return;
}

void Level::set_monitor_state(TestExecType monitor_state_)
{
    monitor_state = monitor_state_;
    if (monitor_state == MONITOR_STATE_PAUSE)
        return;
    if (monitor_state == MONITOR_STATE_PLAY_ALL)
    {
        test_index = 0;
        sim_point_index = 0;
        touched = false;
        substep_index = 0;
        circuit->reset();
        for (int i = 0; i < 4; i++)
            ports[i] = 0;
    }
    current_simpoint = tests[test_index].sim_points[sim_point_index];
}

void Level::touch()
{
    touched = true;
    score_set = false;
    circuit->fast_prepped = false;
    last_price = circuit->get_cost();
//    remove_circles();
}
void Level::set_best_design(LevelSet* new_best)
{
    delete best_design;
    best_design =  new_best;

    unsigned test_count = tests.size();
    for (unsigned t = 0; t < test_count; t++)
    {
        tests[t].best_score = tests[t].last_score;
        for (int i = 0; i < HISTORY_POINT_COUNT; i++)
            tests[t].best_pressure_log[i] = tests[t].last_pressure_log[i];
    }
}

LevelSet::LevelSet(SaveObject* sobj, bool inspect)
{
    read_only = inspect;
    SaveObjectList* slist = sobj->get_list();
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        SaveObject *sobj = NULL;
        if (i < slist->get_count())
        {
            sobj = slist->get_item(i);
            if (sobj->is_null())
                sobj = NULL;
        }
        if (sobj)
            levels[i] = new Level(i, sobj);
        else
        {
            levels[i] = new Level(i, inspect);
        }
    }
    for (int i = 0; i < LEVEL_COUNT; i++)
        remove_circles(i);
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

SaveObject* LevelSet::save_all(unsigned level_index, bool lite)
{
    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < (level_index + 1); i++)
    {
        if (!levels[i]->hidden)
            slist->add_item(levels[i]->save(lite));
        else
            slist->add_item(new SaveObjectNull);
    }
    return slist;
}

SaveObject* LevelSet::save_one(unsigned level_index)
{
    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        if (levels[level_index]->circuit->contains_subcircuit_level(i, this) || i == level_index)
            slist->add_item(levels[i]->save(true));
        else
            slist->add_item(new SaveObjectNull);
    }
    return slist;
}

bool LevelSet::is_playable(unsigned level, unsigned highest_level)
{
#ifdef COMPRESSURE_DEMO
    if (level >= 14)
        return false;
#endif
    if (level >= LEVEL_COUNT)
        return false;
    if (read_only)
        return (!levels[level]->hidden);
    if (level > highest_level)
        return false;
    return true;
}

int LevelSet::top_playable()
{
    if (read_only)
        return -1;
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        if (levels[i]->best_score <= percent_as_pressure(0))
            return i;
    }
    return LEVEL_COUNT - 1;
}

Pressure LevelSet::test_level(unsigned level_index)
{
    reset(level_index);
    levels[level_index]->set_monitor_state(MONITOR_STATE_PLAY_ALL);
    while (!levels[level_index]->score_set)
        levels[level_index]->advance(1000);
    return levels[level_index]->last_score;
}

void LevelSet::record_best_score(unsigned level_index)
{
    SaveObject* sobj = save_one(level_index);
    levels[level_index]->set_best_design(new LevelSet(sobj, true));
    delete sobj;
}

void LevelSet::save_design(unsigned level_index, unsigned save_slot)
{
    SaveObject* sobj = save_one(level_index);
    delete levels[level_index]->saved_designs[save_slot];
    levels[level_index]->saved_designs[save_slot] =  new LevelSet(sobj, true);
    delete sobj;
}

void LevelSet::reset(unsigned level_index)
{
    levels[level_index]->circuit->remove_circles(this);
    levels[level_index]->reset();
}


void LevelSet::remove_circles(unsigned level_index)
{
    if (levels[level_index])
        levels[level_index]->circuit->remove_circles(this);
}

void LevelSet::touch(unsigned level_index)
{
    levels[level_index]->touch();
    levels[level_index]->circuit->remove_circles(this);
}

