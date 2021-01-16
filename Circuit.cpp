#include "Circuit.h"
#include "GameState.h"

#include "SaveState.h"
#include "Level.h"
#include "Misc.h"


SaveObject* CircuitElement::save()
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_num("type", get_type());
    save(omap);
    return omap;
}

CircuitElement* CircuitElement::load(SaveObjectMap* omap)
{
    CircuitElementType type = CircuitElementType(omap->get_num("type"));
    switch (type)
    {
        case CIRCUIT_ELEMENT_TYPE_PIPE:
            return new CircuitElementPipe(omap);
        case CIRCUIT_ELEMENT_TYPE_VALVE:
            return new CircuitElementValve(omap);
        case CIRCUIT_ELEMENT_TYPE_SOURCE:
            return new CircuitElementSource(omap);
        case CIRCUIT_ELEMENT_TYPE_SUBCIRCUIT:
            return new CircuitElementSubCircuit(omap);
        default:
            assert(0);        
    }
}

CircuitElementPipe::CircuitElementPipe(SaveObjectMap* omap)
{
    connections = Connections(omap->get_num("connections"));
}

void CircuitElementPipe::save(SaveObjectMap* omap)
{
    omap->add_num("connections", connections);
}

SDL_Rect CircuitElementPipe::getimage_bg(void)
{
    if (connections == CONNECTIONS_NS)
    {
        int x = pressure * (48 - 6) / PRESSURE_SCALAR / 100;
        int move_by = (moved * 100) / PRESSURE_SCALAR;
        move_by = std::min(move_by, 100);
        move_by = std::max(move_by, -100);
        moved_pos += move_by;
        unsigned offset = unsigned(moved_pos / 10) % (48 - 14);
        return SDL_Rect{256 + x, 192 + int(offset), 6, 14};
    }
    if (connections == CONNECTIONS_EW)
    {
        int y = pressure * (48 - 6) / PRESSURE_SCALAR / 100;
        int move_by = (moved * 100) / PRESSURE_SCALAR;
        move_by = std::min(move_by, 100);
        move_by = std::max(move_by, -100);
        moved_pos += move_by;
        unsigned offset = unsigned(moved_pos / 10) % (48 - 14);
        return SDL_Rect{304 + int(offset), 192 + y, 14, 6};
    }
    return SDL_Rect{0, 0, 0, 0};
}

XYPos CircuitElementPipe::getimage(void)
{
    return XYPos((connections % 4) * 32, (connections / 4) * 32);
}

static void sim_pre_2links(CircuitPressure& a, CircuitPressure& b)
{
    Pressure mov = (a.value - b.value) / 2;
    a.move(-mov);
    b.move(mov);
}


static void sim_pre_3links(CircuitPressure& a, CircuitPressure& b, CircuitPressure& c)
{
    Pressure mov = (a.value - b.value) / 3;
    a.move(-mov);
    b.move(mov);

    mov = (a.value - c.value) / 3;
    a.move(-mov);
    c.move(mov);

    mov = (b.value - c.value) / 3;
    b.move(-mov);
    c.move(mov);
}

static void sim_pre_4links(CircuitPressure& a, CircuitPressure& b, CircuitPressure& c, CircuitPressure& d)
{
    Pressure mov = (a.value - b.value) / 4;
    a.move(-mov);
    b.move(mov);

    mov = (a.value - c.value) / 4;
    a.move(-mov);
    c.move(mov);

    mov = (b.value - c.value) / 4;
    b.move(-mov);
    c.move(mov);

    mov = (a.value - d.value) / 4;
    a.move(-mov);
    d.move(mov);

    mov = (b.value - d.value) / 4;
    b.move(-mov);
    d.move(mov);

    mov = (c.value - d.value) / 4;
    c.move(-mov);
    d.move(mov);
}

void CircuitElementPipe::sim_pre(PressureAdjacent adj)
{

    switch(connections)
    {
        case CONNECTIONS_NONE:
            adj.N.vent(); adj.E.vent(); adj.S.vent(); adj.W.vent(); break;
        case CONNECTIONS_NW:
            sim_pre_2links(adj.N, adj.W); adj.E.vent(); adj.S.vent(); break;
        case CONNECTIONS_NE:
            sim_pre_2links(adj.N, adj.E); adj.W.vent(); adj.S.vent(); break;
        case CONNECTIONS_NS:
        {
            Pressure mov = (adj.S.value - adj.N.value) / 2;
            adj.S.move(-mov);
            adj.N.move(mov);
            pressure = (adj.S.value + adj.N.value) / 2;
            moved = mov;
            adj.E.vent(); adj.W.vent(); break;
        }
        case CONNECTIONS_EW:
        {
            Pressure mov = (adj.E.value - adj.W.value) / 2;
            adj.E.move(-mov);
            adj.W.move(mov);
            pressure = (adj.E.value + adj.W.value) / 2;
            moved = mov;
            adj.N.vent(); adj.S.vent(); break;
        }

            sim_pre_2links(adj.E, adj.W); adj.N.vent(); adj.S.vent(); break;
        case CONNECTIONS_ES:
            sim_pre_2links(adj.E, adj.S); adj.N.vent(); adj.W.vent(); break;
        case CONNECTIONS_WS:
            sim_pre_2links(adj.W, adj.S); adj.N.vent(); adj.E.vent(); break;
        case CONNECTIONS_NWE:
            sim_pre_3links(adj.N, adj.W, adj.E); adj.S.vent(); break;
        case CONNECTIONS_NES:
            sim_pre_3links(adj.N, adj.E, adj.S); adj.W.vent(); break;
        case CONNECTIONS_NWS:
            sim_pre_3links(adj.N, adj.W, adj.S); adj.E.vent(); break;
        case CONNECTIONS_EWS:
            sim_pre_3links(adj.E, adj.W, adj.S); adj.N.vent(); break;
        case CONNECTIONS_NS_WE:
            sim_pre_2links(adj.N, adj.S); sim_pre_2links(adj.E, adj.W); break;
        case CONNECTIONS_NW_ES:
            sim_pre_2links(adj.N, adj.W); sim_pre_2links(adj.E, adj.S); break;
        case CONNECTIONS_NE_WS:
            sim_pre_2links(adj.N, adj.E); sim_pre_2links(adj.W, adj.S); break;
        case CONNECTIONS_ALL:
            sim_pre_4links(adj.N, adj.E, adj.W, adj.S); break;
        default:
            assert(0);
    }
}

static unsigned con_to_bitmap(Connections con)
{
    switch (con)
    {
        case CONNECTIONS_NONE:  return 0;
        case CONNECTIONS_NW:    return 9;
        case CONNECTIONS_NE:    return 3;
        case CONNECTIONS_NS:    return 5;
        case CONNECTIONS_EW:    return 10;
        case CONNECTIONS_ES:    return 6;
        case CONNECTIONS_WS:    return 12;
        case CONNECTIONS_NWE:   return 11;
        case CONNECTIONS_NES:   return 7;
        case CONNECTIONS_NWS:   return 13;
        case CONNECTIONS_EWS:   return 14;
        case CONNECTIONS_NS_WE: return 15;
        case CONNECTIONS_NW_ES: return 15;
        case CONNECTIONS_NE_WS: return 15;
        case CONNECTIONS_ALL:   return 15;
        default: assert(0);
    }
}

static Connections bitmap_to_con(unsigned bit)
{
    switch (bit)
    {
        case 3:    return CONNECTIONS_NE;
        case 5:    return CONNECTIONS_NS;
        case 6:    return CONNECTIONS_ES;
        case 7:    return CONNECTIONS_NES;
        case 9:    return CONNECTIONS_NW;
        case 10:   return CONNECTIONS_EW;
        case 11:   return CONNECTIONS_NWE;
        case 12:   return CONNECTIONS_WS;
        case 13:   return CONNECTIONS_NWS;
        case 14:   return CONNECTIONS_EWS;
        case 15:   return CONNECTIONS_ALL;
        default: assert(0);
    }
}

void CircuitElementPipe::extend_pipe(Connections con)
{
    if (connections == CONNECTIONS_NONE) connections = con;
    else if (con == CONNECTIONS_NONE) connections = CONNECTIONS_NONE;       // Special for "clear"
    else if (con == CONNECTIONS_NS && connections == CONNECTIONS_EW) connections = CONNECTIONS_NS_WE;
    else if (con == CONNECTIONS_EW && connections == CONNECTIONS_NS) connections = CONNECTIONS_NS_WE;
    else if (con == CONNECTIONS_NW && connections == CONNECTIONS_ES) connections = CONNECTIONS_NW_ES;
    else if (con == CONNECTIONS_ES && connections == CONNECTIONS_NW) connections = CONNECTIONS_NW_ES;
    else if (con == CONNECTIONS_NE && connections == CONNECTIONS_WS) connections = CONNECTIONS_NE_WS;
    else if (con == CONNECTIONS_WS && connections == CONNECTIONS_NE) connections = CONNECTIONS_NE_WS;
    else if (connections == CONNECTIONS_NS_WE && (con == CONNECTIONS_NS || con == CONNECTIONS_EW)) return;
    else if (connections == CONNECTIONS_NW_ES && (con == CONNECTIONS_NW || con == CONNECTIONS_ES)) return;
    else if (connections == CONNECTIONS_NE_WS && (con == CONNECTIONS_NE || con == CONNECTIONS_WS)) return;
    else 
    {
        unsigned mask = con_to_bitmap(con) | con_to_bitmap(connections);
        connections = bitmap_to_con(mask);
    }

}

CircuitElementValve::CircuitElementValve(SaveObjectMap* omap)
{
    direction = Direction(omap->get_num("direction"));
}

void CircuitElementValve::save(SaveObjectMap* omap)
{
    omap->add_num("direction", direction);
}

void CircuitElementValve::reset(LevelSet* level_set)
{
    pos_charge = 0;
    neg_charge = 0;
}

XYPos CircuitElementValve::getimage(void)
{
    return XYPos(direction * 32, 4 * 32);
}

void CircuitElementValve::sim_pre(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
    Pressure mov;

    int64_t mul = (pos_charge - neg_charge);
    if (mul < 0)
        mul = 0;

    mov = (pos_charge - adj.N.value * capacity) / (2 * capacity);
    pos_charge -= mov;
    adj.N.move(mov);
    
    mov = (neg_charge - adj.S.value * capacity) / (2 * capacity);
    neg_charge -= mov;
    adj.S.move(mov);
    
    
    mov = (int64_t(adj.W.value - adj.E.value) * mul) / (int64_t(2000) * capacity * PRESSURE_SCALAR);
    adj.W.move(-mov);
    adj.E.move(mov);
}

CircuitElementSource::CircuitElementSource(SaveObjectMap* omap)
{
    direction = Direction(omap->get_num("direction"));
}

void CircuitElementSource::save(SaveObjectMap* omap)
{
    omap->add_num("direction", direction);
}

XYPos CircuitElementSource::getimage(void)
{
    return XYPos(128 + direction * 32, 0);
}

void CircuitElementSource::sim_pre(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
    
    adj.N.move((100 * PRESSURE_SCALAR - adj.N.value)/2);
    adj.E.vent();
    adj.S.vent();
    adj.W.vent();
}

CircuitElementSubCircuit::CircuitElementSubCircuit(Direction direction_, unsigned level_index_, LevelSet* level_set):
    direction(direction_),
    level_index(level_index_)
{
    level = NULL;
    circuit = NULL;
    if (level_set)
        reset(level_set);
}

CircuitElementSubCircuit::CircuitElementSubCircuit(SaveObjectMap* omap)
{
    direction = Direction(omap->get_num("direction"));
    level_index = Direction(omap->get_num("level_index"));
    level = NULL;
    circuit = NULL;

}

void CircuitElementSubCircuit::save(SaveObjectMap* omap)
{
    omap->add_num("direction", direction);
    omap->add_num("level_index", level_index);
}

void CircuitElementSubCircuit::reset(LevelSet* level_set)
{
    level = level_set->levels[level_index];
    if (circuit)
        delete circuit;
    circuit = new Circuit(*level->circuit);
    circuit->reset(level_set);
}

void CircuitElementSubCircuit::retire()
{
    level = NULL;
    circuit = NULL;
}

bool CircuitElementSubCircuit::contains_subcircuit_level(unsigned level_index_q, LevelSet* level_set)
{
    if (level_index_q == level_index)
        return true;
    return level_set->levels[level_index]->circuit->contains_subcircuit_level(level_index_q, level_set);
}

XYPos CircuitElementSubCircuit::getimage(void)
{
    assert(level);
    return level->getimage(direction);
}

XYPos CircuitElementSubCircuit::getimage_fg(void)
{
    assert(level);
    return level->getimage_fg(direction);
}


void CircuitElementSubCircuit::sim_pre(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
    assert(circuit);
    circuit->sim_pre(adj);
}

void CircuitElementSubCircuit::sim_post(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
    assert(circuit);
    circuit->sim_post(adj);
}

Circuit::Circuit(SaveObjectMap* omap)
{
    SaveObjectList* slist_y = omap->get_item("elements")->get_list();
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    {
        SaveObjectList* slist_x = slist_y->get_item(pos.y)->get_list();
        for (pos.x = 0; pos.x < 9; pos.x++)
        {
            SaveObjectMap* smap = slist_x->get_item(pos.x)->get_map();
            elements[pos.y][pos.x] = CircuitElement::load(smap);
        }
    }
}

Circuit::Circuit()
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x] = new CircuitElementPipe();
    }
}

Circuit::Circuit(Circuit& other)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x] = other.elements[pos.y][pos.x]->copy();
    }
}

SaveObject* Circuit::save()
{
    SaveObjectMap* omap = new SaveObjectMap;
    
    SaveObjectList* slist_y = new SaveObjectList;
    XYPos pos;
    
    for (pos.y = 0; pos.y < 9; pos.y++)
    {
        SaveObjectList* slist_x = new SaveObjectList;
        for (pos.x = 0; pos.x < 9; pos.x++)
        {
            slist_x->add_item(elements[pos.y][pos.x]->save());
        }
        slist_y->add_item(slist_x);
    }
    omap->add_item("elements", slist_y);
    return omap;
}

void Circuit::reset(LevelSet* level_set)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        connections_ns[pos.y][pos.x] = 0;
        connections_ew[pos.y][pos.x] = 0;
    }
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x]->reset(level_set);
    }

}

void Circuit::retire()
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x]->retire();
    }

}

void Circuit::sim_pre(PressureAdjacent adj_)
{
    if (!fast_prepped)
    {
        fast_funcs.clear();
        fast_pressures.clear();

        XYPos pos;
        for (pos.y = 0; pos.y < 10; pos.y++)
        for (pos.x = 0; pos.x < 10; pos.x++)
        {
            connections_ns[pos.y][pos.x].pre();
            connections_ew[pos.y][pos.x].pre();
        }


        for (pos.y = 0; pos.y < 9; pos.y++)
        for (pos.x = 0; pos.x < 9; pos.x++)
        {
            PressureAdjacent adj(pos == XYPos(4,0) ? adj_.N : connections_ns[pos.y][pos.x],
                                 pos == XYPos(8,4) ? adj_.E : connections_ew[pos.y][pos.x+1],
                                 pos == XYPos(4,8) ? adj_.S : connections_ns[pos.y+1][pos.x],
                                 pos == XYPos(0,4) ? adj_.W : connections_ew[pos.y][pos.x]);
            elements[pos.y][pos.x]->sim_pre(adj);
        }

        for (pos.y = 0; pos.y < 9; pos.y++)
        for (pos.x = 0; pos.x < 9; pos.x++)
        {
            PressureAdjacent adj(pos == XYPos(4,0) ? adj_.N : connections_ns[pos.y][pos.x],
                                 pos == XYPos(8,4) ? adj_.E : connections_ew[pos.y][pos.x+1],
                                 pos == XYPos(4,8) ? adj_.S : connections_ns[pos.y+1][pos.x],
                                 pos == XYPos(0,4) ? adj_.W : connections_ew[pos.y][pos.x]);
            if (adj.N.touched || adj.E.touched || adj.S.touched || adj.W.touched)
                fast_funcs.push_back(FastFunc(elements[pos.y][pos.x],adj));
        }

        for (pos.y = 0; pos.y < 10; pos.y++)
        for (pos.x = 0; pos.x < 10; pos.x++)
        {
            if (connections_ns[pos.y][pos.x].touched)
                fast_pressures.push_back(&connections_ns[pos.y][pos.x]);
            if (connections_ew[pos.y][pos.x].touched)
                fast_pressures.push_back(&connections_ew[pos.y][pos.x]);
        }
        fast_prepped = true;
    }
    else
    {
        for (CircuitPressure* con : fast_pressures)
        {
            con->pre();
        }
        for (FastFunc& fast_func : fast_funcs)
        {
            fast_func.element->sim_pre(fast_func.adj);
        }
    
    }


}

void Circuit::sim_post(PressureAdjacent adj_)
{
    
    assert(fast_prepped);
    if (!fast_prepped)
    {
        XYPos pos;
        for (pos.y = 0; pos.y < 9; pos.y++)
        for (pos.x = 0; pos.x < 9; pos.x++)
        {
            PressureAdjacent adj(pos == XYPos(4,0) ? adj_.N : connections_ns[pos.y][pos.x],
                                 pos == XYPos(8,4) ? adj_.E : connections_ew[pos.y][pos.x+1],
                                 pos == XYPos(4,8) ? adj_.S : connections_ns[pos.y+1][pos.x],
                                 pos == XYPos(0,4) ? adj_.W : connections_ew[pos.y][pos.x]);
            elements[pos.y][pos.x]->sim_post(adj);
        }

        for (pos.y = 0; pos.y < 10; pos.y++)
        for (pos.x = 0; pos.x < 10; pos.x++)
        {
            connections_ns[pos.y][pos.x].post();
            connections_ew[pos.y][pos.x].post();
        }
    }
    else
    {
        for (FastFunc& fast_func : fast_funcs)
        {
            fast_func.element->sim_post(fast_func.adj);
        }
        for (CircuitPressure* con : fast_pressures)
        {
            con->post();
        }
    }

}


void Circuit::set_element_pipe(XYPos pos, Connections con)
{
    if (elements[pos.y][pos.x]->get_type() == CIRCUIT_ELEMENT_TYPE_PIPE)
    {
        elements[pos.y][pos.x]->extend_pipe(con);
    }
    else
    {
        delete elements[pos.y][pos.x];
        elements[pos.y][pos.x] = new CircuitElementPipe(con);
    }
    ammended();
}

void Circuit::set_element_valve(XYPos pos, Direction direction)
{
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementValve(direction);
    ammended();
}

void Circuit::set_element_source(XYPos pos, Direction direction)
{
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementSource(direction);
    ammended();
}

void Circuit::set_element_subcircuit(XYPos pos, Direction direction, unsigned level_index, LevelSet* level_set)
{
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementSubCircuit(direction, level_index, level_set);
    ammended();
}

bool Circuit::contains_subcircuit_level(unsigned level_index, LevelSet* level_set)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        if (elements[pos.y][pos.x]->contains_subcircuit_level(level_index, level_set))
            return true;
    }
    return false;
}
