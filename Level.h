#pragma once
#include "Misc.h"
#include "SaveState.h"
#include "Circuit.h"

#define LEVEL_COUNT 10

class Level
{
public:
    unsigned level_index;
    Circuit* circuit;

    Level(unsigned level_index_, SaveObject* sobj);
    Level(unsigned level_index_);
    SaveObject* save();
};
