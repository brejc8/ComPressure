#include "Level.h"



Level::Level(unsigned level_index_):
    level_index(level_index_)
{
    circuit = new Circuit;
}

Level::Level(unsigned level_index_, SaveObject* sobj):
    level_index(level_index_)
{
    if (sobj)
        circuit = new Circuit(sobj->get_map()->get_item("circuit")->get_map());
    else
        circuit = new Circuit;
}

SaveObject* Level::save()
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_item("circuit", circuit->save());
    return omap;

}
