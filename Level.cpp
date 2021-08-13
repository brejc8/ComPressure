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

void Test::load(SaveObjectMap* player_map, SaveObjectMap* test_map)
{
    if (player_map && player_map->has_key("last_score"))
    {
        last_score = player_map->get_num("last_score");
        best_score = player_map->get_num("best_score");

        {
            SaveObjectList* slist = player_map->get_item("best_pressure_log")->get_list();
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                best_pressure_log[i] = slist->get_num(i);
        }

        {
            SaveObjectList* slist = player_map->get_item("last_pressure_log")->get_list();
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                last_pressure_log[i] = slist->get_num(i);
        }
        last_pressure_index = player_map->get_num("last_pressure_index");
    }
    
    if (test_map->has_key("tested_direction"))
        tested_direction = Direction(test_map->get_num("tested_direction"));
    if (test_map->has_key("reset"))
        reset = TestResetType(test_map->get_num("reset"));
    if (test_map->has_key("first_simpoint"))
        first_simpoint = test_map->get_num("first_simpoint");

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
        else point.force[0] = tested_direction == DIRECTION_N ? 0 : 50;
        if (point_map->has_key("EF")) point.force[1] = point_map->get_num("EF");
        else point.force[1] = tested_direction == DIRECTION_E ? 0 : 50;
        if (point_map->has_key("SF")) point.force[2] = point_map->get_num("SF");
        else point.force[2] = tested_direction == DIRECTION_S ? 0 : 50;
        if (point_map->has_key("WF")) point.force[3] = point_map->get_num("WF");
        else point.force[3] = tested_direction == DIRECTION_W ? 0 : 50;

        if (point_map->has_key("PRESET"))
            first_simpoint = j + 1;

        sim_points.push_back(point);
    }

}

SaveObject* Test::save(bool custom, bool lite)
{
    SaveObjectMap* omap = new SaveObjectMap;
    if (!lite)
    {
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
    }
    if (custom)
    {
        if (tested_direction != DIRECTION_E)
            omap->add_num("tested_direction", tested_direction);
        SaveObjectList* slist = new SaveObjectList;
        for (SimPoint& sp: sim_points)
        {
            SaveObjectMap* sp_map = new SaveObjectMap;
            if (sp.values[0]) sp_map->add_num("N", sp.values[0]);
            if (sp.values[1]) sp_map->add_num("E", sp.values[1]);
            if (sp.values[2]) sp_map->add_num("S", sp.values[2]);
            if (sp.values[3]) sp_map->add_num("W", sp.values[3]);
            if (sp.force[0] != ((tested_direction == DIRECTION_N) ? 0 : 50)) sp_map->add_num("NF", sp.force[0]);
            if (sp.force[1] != ((tested_direction == DIRECTION_E) ? 0 : 50)) sp_map->add_num("EF", sp.force[1]);
            if (sp.force[2] != ((tested_direction == DIRECTION_S) ? 0 : 50)) sp_map->add_num("SF", sp.force[2]);
            if (sp.force[3] != ((tested_direction == DIRECTION_W) ? 0 : 50)) sp_map->add_num("WF", sp.force[3]);
            slist->add_item(sp_map);
        }
        omap->add_item("points", slist);
        if (reset)
            omap->add_num("reset", reset);
        if (first_simpoint)
            omap->add_num("first_simpoint", first_simpoint);
    }

    return omap;
}

Level::Level(int level_index_, bool hidden_):
    level_index(level_index_),
    hidden(hidden_)
{
    pin_order[0] = -1; pin_order[1] = -1; pin_order[2] = -1; pin_order[3] = -1;
    circuit = new Circuit;
    init_tests();
}

Level::Level(int level_index_, SaveObject* sobj, unsigned version, bool inspected_):
    level_index(level_index_),
    inspected(inspected_)
{
    pin_order[0] = -1; pin_order[1] = -1; pin_order[2] = -1; pin_order[3] = -1;
    SaveObjectMap* omap = sobj->get_map();
    circuit = new Circuit(omap->get_item("circuit")->get_map());
    if (omap->has_key("best_design"))
        best_design = new LevelSet(omap->get_item("best_design"), version, true);
    if (omap->has_key("saved_designs"))
    {
        SaveObjectList* slist = omap->get_item("saved_designs")->get_list();
        for (unsigned i = 0; i < 4; i++)
        {
            if (i < slist->get_count())
            {
                SaveObject *sobj = slist->get_item(i);
                if (!sobj->is_null())
                    saved_designs[i] = new LevelSet(sobj, version, true);
            }
        }
    }

    init_tests(omap);
}

Level::~Level()
{
    delete circuit;
    delete best_design;
    delete help_design;
    for (unsigned i = 0; i < 4; i++)
        delete saved_designs[i];
    delete texture;
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
        if (best_design)
            omap->add_item("best_design", best_design->save_all(LEVEL_COUNT, true));

        SaveObjectList* slist = new SaveObjectList;
        for (unsigned i = 0; i < 4; i++)
            if (saved_designs[i])
                slist->add_item(saved_designs[i]->save_all(LEVEL_COUNT, true));
            else
                slist->add_item(new SaveObjectNull);
        omap->add_item("saved_designs", slist);
    }
    else
    {
        omap->add_num("best_score", score_set ? last_score : 0);
    }
    if (!lite || level_index >= LEVEL_COUNT)
    {
        SaveObjectList* slist = new SaveObjectList;
        unsigned test_count = tests.size();
        for (unsigned i = 0; i < test_count; i++)
            slist->add_item(tests[i].save(level_index >= LEVEL_COUNT, lite));
        omap->add_item("tests", slist);
    }

    if (level_index >= LEVEL_COUNT)
    {
        if (description != "")
            omap->add_string("description", description);
        if (global)
            omap->add_num("global", 1);
        omap->add_string("name", name);
        {
            SaveObjectList* slist = new SaveObjectList;
            for (int i = 0; i < 4; i++)
            {
                if (pin_order[i] < 0)
                    break;
                slist->add_num(pin_order[i]);
            }
            omap->add_item("connections", slist);
        }
        omap->add_num("substep_count", substep_count);
        omap->add_item("forced_elements", circuit->save_forced());
        
        uint8_t base_pixels[24][24];

        SaveObjectList* y_list = new SaveObjectList;
        for (unsigned y = 0; y < 24; y++)
        {
            SaveObjectList* x_list = new SaveObjectList;
            for (unsigned x = 0; x < 24; x++)
            {
                int colour = icon_pixels[y][x];
                for (int i = 0; i < 8; i++)
                {
                    XYPos npos = DirFlip(i).trans(XYPos(x,y), 24);
                    if (colour != icon_pixels[npos.y][npos.x + i * 24])
                    {
                        colour = 8;
                        break;
                    }
                }
                base_pixels[y][x] = colour;
                x_list->add_num(colour);
            }
            while (x_list->get_count() && x_list->get_num(x_list->get_count() - 1) == 8)
                x_list->pop_back();
            y_list->add_item(x_list);
        }
        omap->add_item("icon_bg", y_list);

        y_list = new SaveObjectList;
        for (unsigned y = 0; y < 24; y++)
        {
            SaveObjectList* x_list = new SaveObjectList;
            for (unsigned x = 0; x < 24*8; x++)
            {
                XYPos npos = DirFlip(x / 24).trans_inv(XYPos(x%24,y), 24);
                if (base_pixels[npos.y][npos.x] != icon_pixels[y][x])
                    x_list->add_num(icon_pixels[y][x]);
                else
                    x_list->add_num(8);
            }
            while (x_list->get_count() && x_list->get_num(x_list->get_count() - 1) == 8)
                x_list->pop_back();
            y_list->add_item(x_list);
        }
        omap->add_item("icon", y_list);
        if (!dialogue.empty())
        {
            SaveObjectList* dia_list = new SaveObjectList;
            for (DialogueScreen& dia: dialogue)
            {
                SaveObjectMap* dia_entry = new SaveObjectMap;
                dia_entry->add_string("text", dia.text);
                dia_entry->add_string("who", dia.who);
                dia_list->add_item(dia_entry);
            }
            omap->add_item("dialogue", dia_list);
        }
        if (!hints.empty())
        {
            SaveObjectList* dia_list = new SaveObjectList;
            for (DialogueScreen& dia: hints)
            {
                SaveObjectMap* dia_entry = new SaveObjectMap;
                dia_entry->add_string("text", dia.text);
                dia_entry->add_string("who", dia.who);
                dia_list->add_item(dia_entry);
            }
            omap->add_item("hints", dia_list);
        }

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
    if (level_index < LEVEL_COUNT)
        return XYPos(dir_flip.as_int() * 24, 184 + (int(level_index) * 24));
    return XYPos(dir_flip.as_int() * 24, 0);
}

WrappedTexture* Level::getimage_fg_texture()
{
    return texture;
}

void Level::setimage_fg_texture(WrappedTexture* texture_)
{
    delete texture;
    texture = texture_;
}

void Level::init_tests(SaveObjectMap* omap)
{
    SaveObjectList* slist = NULL;
    if (omap && omap->has_key("tests"))
        slist = omap->get_item("tests")->get_list();

    unsigned loaded_level_version = 0;
    if (omap && omap->has_key("level_version"))
        loaded_level_version = omap->get_num("level_version");
    
    SaveObjectMap* desc = level_index < LEVEL_COUNT ? level_desc->get_item(level_index)->get_map() : omap;

    if (desc)
    {
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
        if (desc->has_key("help_design") && !inspected && !hidden)
            help_design = new LevelSet(desc->get_item("help_design"), COMPRESSURE_VERSION, true);
        if (desc->has_key("global"))
            global = true;
        if (desc->has_key("description"))
            description = desc->get_string("description");

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
            SaveObjectMap* test_map = testlist->get_item(i)->get_map();
            SaveObjectMap* player_map;

            if (loaded_level_version == level_version && slist && slist->get_count() > i)
                player_map = slist->get_item(i)->get_map();
            else
                player_map = NULL;
            
            t.load(player_map, test_map);
        }

        for (unsigned y = 0; y < 24; y++)
            for (unsigned x = 0; x < 24*8; x++)
                icon_pixels[y][x] = 8;

        if (desc->has_key("icon_bg"))
        {
            SaveObjectList* icon_list_y = desc->get_item("icon_bg")->get_list();
            for (unsigned y = 0; y < icon_list_y->get_count() && y < 24; y++)
            {
                SaveObjectList* icon_list_x = icon_list_y->get_item(y)->get_list();
                for (unsigned x = 0; x < icon_list_x->get_count() && x < 24; x++)
                {
                    for (int i = 0; i < 8; i++)
                    {
                        XYPos pos = DirFlip(i).trans(XYPos(x,y), 24);
                        icon_pixels[pos.y][pos.x + i * 24] = icon_list_x->get_num(x);
                    }
                }
            }
        }
        if (desc->has_key("icon"))
        {
            SaveObjectList* icon_list_y = desc->get_item("icon")->get_list();
            for (unsigned y = 0; y < icon_list_y->get_count() && y < 24; y++)
            {
                SaveObjectList* icon_list_x = icon_list_y->get_item(y)->get_list();
                for (unsigned x = 0; x < icon_list_x->get_count() && x < 24*8; x++)
                {
                    uint8_t colour = icon_list_x->get_num(x);
                    if (colour != 8)
                        icon_pixels[y][x] = colour;
                }
            }
        }
        if (desc->has_key("dialogue"))
        {
            SaveObjectList* dialogue_list = desc->get_item("dialogue")->get_list();
            for (unsigned i = 0; i < dialogue_list->get_count(); i++)
            {
                SaveObjectMap* entry = dialogue_list->get_item(i)->get_map();
                dialogue.push_back(DialogueScreen{entry->get_string("who"), entry->get_string("text")});
            }
        }
        if (desc->has_key("hints"))
        {
            SaveObjectList* dialogue_list = desc->get_item("hints")->get_list();
            for (unsigned i = 0; i < dialogue_list->get_count(); i++)
            {
                SaveObjectMap* entry = dialogue_list->get_item(i)->get_map();
                hints.push_back(DialogueScreen{entry->get_string("who"), entry->get_string("text")});
            }
        }
    }
    else
    {
        name = "New level";
        connection_mask = 2;
        pin_order[0] = DIRECTION_E;
        pin_order[1] = -1;
        pin_order[2] = -1;
        pin_order[3] = -1;
        substep_count = 10000;
        level_version = 0;

        tests.push_back({});
        Test& t = tests.back();
        SimPoint point;
        t.sim_points.push_back(point);


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

void Level::re_init_tests(SaveObjectMap* desc)
{
        substep_count = desc->get_num("substep_count");
        tests.clear();
        SaveObjectList* testlist = desc->get_item("tests")->get_list();
        for (unsigned i = 0; i < testlist->get_count(); i++)
        {
            tests.push_back({});
            Test& t = tests.back();
            SaveObjectMap* test_map = testlist->get_item(i)->get_map();
            t.load(NULL, test_map);
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
                    if ((monitor_state == MONITOR_STATE_PLAY_1 && tests[test_index].reset) ||
                        (monitor_state == MONITOR_STATE_PLAY_ALL && tests[test_index].reset == RESET_ALL) || 
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
            test_pressure_histroy_index = (test_pressure_histroy_index + 1) % 200;
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
    if (tests[test_index].reset || test_index == 0)
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

LevelSet::LevelSet(SaveObject* sobj, unsigned version, bool inspect)
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
            levels.push_back(new Level(i, sobj, version, inspect));
        else
            levels.push_back(new Level(i, inspect));
    }
    for (int i = LEVEL_COUNT; i < slist->get_count(); i++)
    {
        SaveObject *sobj = slist->get_item(i);
        if (!sobj->is_null())
            levels.push_back(new Level(i, sobj, version, inspect));
        else
            levels.push_back(new Level(i, inspect));
    }

    for (int i = 0; i < levels.size(); i++)
        remove_circles(i);

}

LevelSet::LevelSet()
{
        for (int i = 0; i < LEVEL_COUNT; i++)
        {
            levels.push_back(new Level(i));
        }
}

LevelSet::~LevelSet()
{
        for (int i = 0; i < levels.size(); i++)
        {
            delete levels[i];
        }
}

SaveObject* LevelSet::save_all(int level_index, bool lite)
{
    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < levels.size(); i++)
    {
        if (is_playable(i, level_index))
            slist->add_item(levels[i]->save(lite));
        else
            slist->add_item(new SaveObjectNull);
    }
    return slist;
}

SaveObject* LevelSet::save_one(int level_index)
{
    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < levels.size(); i++)
    {
        if (levels[level_index]->circuit->contains_subcircuit_level(i, this) || i == level_index)
            slist->add_item(levels[i]->save(true));
        else
            slist->add_item(new SaveObjectNull);
    }
    while (slist->get_count() && slist->get_item(slist->get_count() - 1)->is_null())
        slist->pop_back();


    return slist;
}

bool LevelSet::is_playable(unsigned level, unsigned highest_level)
{
    if (level >= levels.size())
        return false;
    if (read_only)
        return (!levels[level]->hidden);
    if (level < LEVEL_COUNT && level > highest_level)
        return false;
    return true;
}

int LevelSet::top_playable(int highest_level)
{
    if (levels.size() > LEVEL_COUNT)
        return levels.size() - 1;

    return highest_level;
}

Pressure LevelSet::test_level(int level_index)
{
    reset(level_index);
    levels[level_index]->set_monitor_state(MONITOR_STATE_PLAY_ALL);
    while (!levels[level_index]->score_set)
        levels[level_index]->advance(1000);
    return levels[level_index]->last_score;
}

void LevelSet::record_best_score(int level_index)
{
    SaveObject* sobj = save_one(level_index);
    levels[level_index]->set_best_design(new LevelSet(sobj, COMPRESSURE_VERSION, true));
    delete sobj;
}

void LevelSet::save_design(int level_index, unsigned save_slot)
{
    SaveObject* sobj = save_one(level_index);
    delete levels[level_index]->saved_designs[save_slot];
    levels[level_index]->saved_designs[save_slot] =  new LevelSet(sobj, COMPRESSURE_VERSION, true);
    delete sobj;
}

void LevelSet::reset(int level_index)
{
    levels[level_index]->circuit->remove_circles(this);
    levels[level_index]->reset();
}


void LevelSet::remove_circles(int level_index)
{
    if (levels[level_index])
        levels[level_index]->circuit->remove_circles(this);
}

void LevelSet::touch(int level_index)
{
    levels[level_index]->touch();
    levels[level_index]->circuit->remove_circles(this);
}


unsigned LevelSet::new_user_level()
{
    unsigned count = levels.size();
    levels.push_back(new Level(count));
    return count;
}

unsigned LevelSet::import_level(LevelSet* other_set, int level_index)
{
    unsigned new_index = LEVEL_COUNT;
    Level* old_level = other_set->levels[level_index];

    while (true)
    {
        if (new_index >= levels.size())
        {
            levels.push_back(new Level(new_index));
            break;
        }
        if (old_level->name == levels[new_index]->name)
            break;
        new_index++;
    }

    Level* new_level = levels[new_index];
    new_level->name = old_level->name;
    
    for (unsigned y = 0; y < 24; y++)
        for (unsigned x = 0; x < 8*24; x++)
        {
            new_level->icon_pixels[y][x] = old_level->icon_pixels[y][x];
        }
    
    new_level->tests = old_level->tests;
    new_level->circuit->copy_in(old_level->circuit);
    new_level->global = old_level->global;
    new_level->description = old_level->description;
    new_level->substep_count = old_level->substep_count;
    new_level->dialogue = old_level->dialogue;
    new_level->hints = old_level->hints;

    for (unsigned i = 0; i < 4; i++)
        new_level->pin_order[i] = old_level->pin_order[i];
        
    new_level->connection_mask = old_level->connection_mask;

    return new_index;
}

int LevelSet::find_custom_by_name(std::string name)
{
    unsigned new_index = LEVEL_COUNT;
    while (true)
    {
        if (new_index >= levels.size())
            return -1;
        if (name == levels[new_index]->name)
            return new_index;
        new_index++;
    }
}

void LevelSet::delete_level(int level_index)

{
    for (int i = 0; i < levels.size(); i++)
        levels[i]->circuit->reindex_deleted_level(this, level_index);

    delete levels[level_index];
    levels.erase(levels.begin() + level_index);
}

int LevelSet::find_level(int level_index, std::string name)
{
    if (level_index < LEVEL_COUNT)
        return level_index;
    if (level_index < levels.size() && (name == "" || levels[level_index]->name == name))
        return level_index;
    for (int i = LEVEL_COUNT; i < levels.size(); i++)
        if (levels[i]->name == name)
            return i;
    return -1;
}
