#include "Circuit.h"
#include "GameState.h"

#include "SaveState.h"
#include "Level.h"
#include "Misc.h"

void FastSim::FastSimValve::sim()
{
    valve.sim(adj);
}

void FastSim::add_valve(CircuitElementValve& valve, PressureAdjacent adj)
{
    valves.push_back(FastSimValve(valve, adj));
}

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
            if (omap->get_num("connections") == 0)
                return new CircuitElementEmpty(omap);
            else
                return new CircuitElementPipe(omap);
        case CIRCUIT_ELEMENT_TYPE_VALVE:
            return new CircuitElementValve(omap);
        case CIRCUIT_ELEMENT_TYPE_SOURCE:
            return new CircuitElementSource(omap);
        case CIRCUIT_ELEMENT_TYPE_SUBCIRCUIT:
            return new CircuitElementSubCircuit(omap);
        case CIRCUIT_ELEMENT_TYPE_EMPTY:
            return new CircuitElementEmpty(omap);
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

CircuitElementPipe::CircuitElementPipe(SaveObjectMap* omap)
{
    connections = Connections(omap->get_num("connections"));
}

void CircuitElementPipe::save(SaveObjectMap* omap)
{
    omap->add_num("connections", connections);
}

uint16_t CircuitElementPipe::get_desc()
{
    return connections;
}

SDL_Rect CircuitElementPipe::getimage_bg(void)
{
    if (connections == CONNECTIONS_NS)
    {
        int x = (pressure_as_percent(pressure) * (48 - 6)) / 101;
        unsigned offset = unsigned(moved_pos / 50) % (48 - 14);
        return SDL_Rect{256 + x, 208 + int(offset), 6, 14};
    }
    if (connections == CONNECTIONS_EW || connections == CONNECTIONS_NS_WE)
    {
        int y = (pressure_as_percent(pressure) * (48 - 6)) / 101;
        unsigned offset = unsigned(moved_pos / 50) % (48 - 14);
        return SDL_Rect{304 + int(offset), 208 + y, 14, 6};
    }
    return SDL_Rect{0, 0, 0, 0};
}

unsigned CircuitElementPipe::getconnections(void)
{
    return con_to_bitmap(connections);
}

void CircuitElementPipe::render_prep(PressureAdjacent adj)
{
    if (connections == CONNECTIONS_NS)
    {
        Pressure moved = (adj.S.value - adj.N.value) / 2;
        pressure = (adj.S.value + adj.N.value) / 2;
        int move_by = (moved * 100) / PRESSURE_SCALAR;
        move_by = std::min(move_by, 100);
        move_by = std::max(move_by, -100);
        moved_pos += move_by;
    }

    if (connections == CONNECTIONS_EW || connections == CONNECTIONS_NS_WE)
    {
        Pressure moved = (adj.E.value - adj.W.value) / 2;
        pressure = (adj.E.value + adj.W.value) / 2;
        int move_by = (moved * 100) / PRESSURE_SCALAR;
        move_by = std::min(move_by, 100);
        move_by = std::max(move_by, -100);
        moved_pos += move_by;
    }
}

void CircuitElementPipe::sim_prep(PressureAdjacent adj, FastSim& fast_sim)
{
    switch(connections)
    {
        case CONNECTIONS_NONE:
            break;
        case CONNECTIONS_NW:
            fast_sim.add_pipe2(adj.N, adj.W); break;
        case CONNECTIONS_NE:
            fast_sim.add_pipe2(adj.N, adj.E); break;
        case CONNECTIONS_NS:
            fast_sim.add_pipe2(adj.N, adj.S); break;
        case CONNECTIONS_EW:
            fast_sim.add_pipe2(adj.E, adj.W); break;
        case CONNECTIONS_ES:
            fast_sim.add_pipe2(adj.E, adj.S); break;
        case CONNECTIONS_WS:
            fast_sim.add_pipe2(adj.W, adj.S); break;
        case CONNECTIONS_NWE:
            fast_sim.add_pipe3(adj.N, adj.W, adj.E); break;
        case CONNECTIONS_NES:
            fast_sim.add_pipe3(adj.N, adj.E, adj.S); break;
        case CONNECTIONS_NWS:
            fast_sim.add_pipe3(adj.N, adj.W, adj.S); break;
        case CONNECTIONS_EWS:
            fast_sim.add_pipe3(adj.E, adj.W, adj.S); break;
        case CONNECTIONS_NS_WE:
            fast_sim.add_pipe2(adj.E, adj.W); fast_sim.add_pipe2(adj.N, adj.S); break;
        case CONNECTIONS_NW_ES:
            fast_sim.add_pipe2(adj.N, adj.W); fast_sim.add_pipe2(adj.E, adj.S); break;
        case CONNECTIONS_NE_WS:
            fast_sim.add_pipe2(adj.N, adj.E); fast_sim.add_pipe2(adj.W, adj.S); break;
        case CONNECTIONS_ALL:
            fast_sim.add_pipe4(adj.N, adj.E, adj.W, adj.S); break;
        default:
            assert(0);
    }
}

Pressure CircuitElementPipe::get_moved(PressureAdjacent adj)
{
    switch(connections)
    {
        case CONNECTIONS_NW:
            return abs((adj.N.value - adj.W.value) / 2);
        case CONNECTIONS_NE:
            return abs((adj.N.value - adj.E.value) / 2);
        case CONNECTIONS_NS:
            return abs((adj.N.value - adj.S.value) / 2);
        case CONNECTIONS_EW:
            return abs((adj.E.value - adj.W.value) / 2);
        case CONNECTIONS_ES:
            return abs((adj.E.value - adj.S.value) / 2);
        case CONNECTIONS_WS:
            return abs((adj.W.value - adj.S.value) / 2);
        case CONNECTIONS_NS_WE:
            return abs((adj.E.value - adj.W.value) / 2) + abs((adj.N.value - adj.S.value) / 2);
        case CONNECTIONS_NW_ES:
            return abs((adj.N.value - adj.W.value) / 2) + abs((adj.E.value - adj.S.value) / 2);
        case CONNECTIONS_NE_WS:
            return abs((adj.N.value - adj.E.value) / 2) + abs((adj.W.value - adj.S.value) / 2);
        case CONNECTIONS_NONE:
        case CONNECTIONS_NWE:
        case CONNECTIONS_NES:
        case CONNECTIONS_NWS:
        case CONNECTIONS_EWS:
        case CONNECTIONS_ALL:
        default:
            return 0;
    }
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

uint16_t CircuitElementValve::get_desc()
{
    return direction;
}

void CircuitElementValve::reset()
{
}

SDL_Rect CircuitElementValve::getimage_bg(void)
{
    if (direction == DIRECTION_E || direction == DIRECTION_W)
    {
        int x = (pressure_as_percent(pressure) * (48 - 6)) / 101;
        unsigned offset = unsigned(moved_pos / 50) % (48 - 14);
        return SDL_Rect{256 + x, 208 + int(offset), 6, 14};
    }
    else
    {
        
        int y = (pressure_as_percent(pressure) * (48 - 6)) / 101;
        unsigned offset = unsigned(moved_pos / 50) % (48 - 14);
        return SDL_Rect{304 + int(offset), 208 + y, 14, 6};
    }
}

XYPos CircuitElementValve::getimage_fg(void)
{
    if (direction == DIRECTION_E || direction == DIRECTION_W)
        return XYPos(472 + openness * 24, 240);
    else
        return XYPos(472 + openness * 24, 240 + 24);
}

XYPos CircuitElementValve::getimage(void)
{
    return XYPos(direction * 32, 4 * 32);
}

void CircuitElementValve::render_prep(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
    int percent = pressure_as_percent((adj.N.value - adj.S.value));
    if (percent < 5)
        openness = 0;
    else if (percent < 10)
        openness = 1;
    else if (percent < 20)
        openness = 2;
    else if (percent < 40)
        openness = 3;
    else if (percent < 60)
        openness = 4;
    else if (percent < 80)
        openness = 5;
    else 
        openness = 6;

    int64_t mul = (adj.N.value - adj.S.value);
    if (mul < 0)
        mul = 0;
    Pressure mov = (int64_t(adj.W.value - adj.E.value) * mul) / (int64_t(100) * 2 * resistence * PRESSURE_SCALAR);
    
    pressure = (adj.W.value + adj.E.value) / 2;

    int move_by = (mov * 100) / PRESSURE_SCALAR;
    if (direction == DIRECTION_E || direction == DIRECTION_N)
        move_by = -move_by;
    move_by = std::min(move_by, 100);
    move_by = std::max(move_by, -100);
    moved_pos += move_by;
     
}

void CircuitElementValve::sim_prep(PressureAdjacent adj_, FastSim& fast_sim)
{
     PressureAdjacent adj(adj_, direction);
     fast_sim.add_valve(*this, adj);
}
void CircuitElementValve::sim(PressureAdjacent adj)
{
    int64_t mul = (adj.N.value - adj.S.value);
    if (mul < 0)
        mul = 0;

                                                            // base resistence is 10 pipes
    Pressure mov = (int64_t(adj.W.value - adj.E.value) * mul) / (int64_t(100) * 2 * resistence * PRESSURE_SCALAR);
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

uint16_t CircuitElementSource::get_desc()
{
    return direction;
}

unsigned CircuitElementSource::getconnections(void)
{
    return 1 << direction;
}

XYPos CircuitElementSource::getimage(void)
{
    return XYPos(128 + direction * 32, 0);
}

void CircuitElementSource::sim_prep(PressureAdjacent adj_, FastSim& fast_sim)
{
    PressureAdjacent adj(adj_, direction);
    fast_sim.add_source(adj.N);
}

CircuitElementEmpty::CircuitElementEmpty(SaveObjectMap* omap)
{
}

void CircuitElementEmpty::save(SaveObjectMap* omap)
{
}

uint16_t CircuitElementEmpty::get_desc()
{
    return 0;
}

XYPos CircuitElementEmpty::getimage(void)
{
    return XYPos(0, 0);
}

CircuitElementSubCircuit::~CircuitElementSubCircuit()
{
    if (circuit)
        delete circuit;
}

CircuitElementSubCircuit::CircuitElementSubCircuit(Direction direction_, unsigned level_index_, LevelSet* level_set):
    direction(direction_),
    level_index(level_index_)
{
    level = NULL;
    circuit = NULL;
    if (level_set)
    {
        elaborate(level_set);
    }
}

CircuitElementSubCircuit::CircuitElementSubCircuit(SaveObjectMap* omap)
{
    direction = Direction(omap->get_num("direction"));
    level_index = Direction(omap->get_num("level_index"));
    level = NULL;
    circuit = NULL;
    if (omap->has_key("circuit"))
    {
        circuit = new Circuit(omap->get_item("circuit")->get_map());
        custom = true;
    }
}

CircuitElementSubCircuit::CircuitElementSubCircuit(CircuitElementSubCircuit& other)
{
    direction = other.direction;
    level_index = other.level_index;
    level = other.level;
    custom  = other.custom;
    if (custom)
    {
        circuit = new Circuit(*other.circuit);
    }
}

void CircuitElementSubCircuit::save(SaveObjectMap* omap)
{
    omap->add_num("direction", direction);
    omap->add_num("level_index", level_index);
    if (custom)
    {
        omap->add_item("circuit", circuit->save());
    }
}

uint16_t CircuitElementSubCircuit::get_desc()
{
    return (level_index << 3) | direction << 1 | custom;
}

CircuitElement* CircuitElementSubCircuit::copy()
{
    return new CircuitElementSubCircuit(*this);
}


void CircuitElementSubCircuit::reset()
{
    circuit->reset();
};

void CircuitElementSubCircuit::elaborate(LevelSet* level_set)
{
    level = level_set->levels[level_index];
    if (!custom)
    {
        if (circuit)
            delete circuit;
        level->circuit->remove_circles(level_set);
        circuit = new Circuit(*level->circuit);
    }
    assert(circuit);
    circuit->elaborate(level_set);
};

void CircuitElementSubCircuit::retire()
{
    if (!custom && circuit)
    {
        delete circuit;
        circuit = NULL;
    }
}

bool CircuitElementSubCircuit::contains_subcircuit_level(unsigned level_index_q, LevelSet* level_set)
{
    if (level_index_q == level_index)
        return true;
    if (circuit)
        return circuit->contains_subcircuit_level(level_index_q, level_set);
    else
        return level_set->levels[level_index]->circuit->contains_subcircuit_level(level_index_q, level_set);
}

unsigned CircuitElementSubCircuit::getconnections(void)
{
    unsigned con = 0;
    con |= circuit->elements[0][4]->getconnections() & (1 << DIRECTION_N);
    con |= circuit->elements[4][8]->getconnections() & (1 << DIRECTION_E);
    con |= circuit->elements[8][4]->getconnections() & (1 << DIRECTION_S);
    con |= circuit->elements[4][0]->getconnections() & (1 << DIRECTION_W);
    
    con <<= direction;
    con |= con >> 4;
    con &= 0xF;
    return con;
}

SDL_Rect CircuitElementSubCircuit::getimage_bg(void)
{
    return SDL_Rect{custom ? 232 : 208, 160, 24, 24};
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

void CircuitElementSubCircuit::sim_prep(PressureAdjacent adj_, FastSim& fast_sim)
{
    static CircuitPressure def;
    PressureAdjacent adj(PressureAdjacent(adj_, getconnections(), def), direction);

    assert(circuit);
    circuit->sim_prep(adj, fast_sim);
}

Sign::Sign(XYPos pos_, Direction direction_, std::string text_):
    pos(pos_),
    direction(direction_),
    text(text_)
{
    if (pos.x < 0)
        pos.x = 0;
    if (pos.y < 0)
        pos.y = 0;
    if (pos.x > 9 * 32 - 1)
        pos.x = 9 * 32 - 1;
    if (pos.y > 9 * 32 - 1)
        pos.y = 9 * 32 - 1;
        
}

Sign::Sign(SaveObject* sobj)
{
    SaveObjectMap* omap = sobj->get_map();
    omap->get_string("text", text);
    omap->get_num("pos.x", pos.x);
    omap->get_num("pos.y", pos.y);
    direction = Direction(omap->get_num("direction"));
    
}

SaveObject*  Sign::save()
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_string("text", text);
    omap->add_num("pos.x", pos.x);
    omap->add_num("pos.y", pos.y);
    omap->add_num("direction", direction);
    return omap;
}

XYPos Sign::get_size()
{
    return size;
}

void Sign::set_size(XYPos size_)
{
    if (size_.x < 14)
        size_.x = 14;
    if (size_.y < 14)
        size_.y = 14;
    size = size_ + XYPos(8,8);
}

XYPos Sign::get_pos()
{
    XYPos box_pos;
    switch (direction)
    {
    case DIRECTION_N:
        box_pos.x = pos.x - size.x / 2;
        box_pos.y = pos.y + 4;
        break;
    case DIRECTION_E:
        box_pos.x = pos.x - size.x - 4;
        box_pos.y = pos.y - size.y / 2;
        break;
    case DIRECTION_S:
        box_pos.x = pos.x - size.x / 2;
        box_pos.y = pos.y - size.y - 4;
        break;
    case DIRECTION_W:
        box_pos.x = pos.x + 4;
        box_pos.y = pos.y - size.y / 2;
        break;
    default:
        assert(0);
    }
    return box_pos;
}

void Sign::rotate(bool clockwise)
{
    if (clockwise)
        direction = Direction((direction + 1) % 4);
    else
        direction = Direction((direction + 3) % 4);
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
    if (omap->has_key("signs"))
    {
        SaveObjectList* slist = omap->get_item("signs")->get_list();
        for (unsigned i = 0; i < slist->get_count(); i++)
        {
            signs.push_back(Sign(slist->get_item(i)));
        }
    }

    
}

Circuit::Circuit()
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x] = new CircuitElementEmpty();
    }
}

Circuit::Circuit(Circuit& other) :
    signs(other.signs)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x] = other.elements[pos.y][pos.x]->copy();
    }


}

Circuit::~Circuit()
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        delete elements[pos.y][pos.x];
    }
    for (const Circuit* c: undo_list)
        delete c;
    for (const Circuit* c: redo_list)
        delete c;
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
    
    SaveObjectList* slist = new SaveObjectList;
    for (Sign &sign : signs)
    {
        slist->add_item(sign.save());
    }
    omap->add_item("signs", slist);

    return omap;
}

void Circuit::copy_elements(Circuit& other)
{
    signs = other.signs;
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        if (elements[pos.y][pos.x]->get_type() != other.elements[pos.y][pos.x]->get_type()
         || elements[pos.y][pos.x]->get_desc() != other.elements[pos.y][pos.x]->get_desc())
        {
            delete elements[pos.y][pos.x];
            elements[pos.y][pos.x] = other.elements[pos.y][pos.x]->copy();
        }
    }
}

void Circuit::reset()
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
        elements[pos.y][pos.x]->reset();
    }
    fast_prepped = false;
}

void Circuit::elaborate(LevelSet* level_set)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x]->elaborate(level_set);
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

void Circuit::render_prep()
{
    XYPos pos;

    last_vented = 0;
    last_moved = 0;

    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        PressureAdjacent adjl(connections_ns[pos.y][pos.x],
                                 connections_ew[pos.y][pos.x+1],
                                 connections_ns[pos.y+1][pos.x],
                                 connections_ew[pos.y][pos.x]);
        elements[pos.y][pos.x]->render_prep(adjl);
        
        last_moved += elements[pos.y][pos.x]->get_moved(adjl);
    }
    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        if (touched_ns[pos.y][pos.x] == 1)
            last_vented += connections_ns[pos.y][pos.x].value;
        if (touched_ew[pos.y][pos.x] == 1)
            last_vented += connections_ew[pos.y][pos.x].value;
    }


}

void Circuit::sim_prep(PressureAdjacent adj, FastSim& fast_sim)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        touched_ns[pos.y][pos.x] = 0;
        touched_ew[pos.y][pos.x] = 0;
    }

    touched_ns[0][4] = 1;
    touched_ew[4][9] = 1;
    touched_ns[9][4] = 1;
    touched_ew[4][0] = 1;

    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        unsigned con = elements[pos.y][pos.x]->getconnections();
        if ((con >> DIRECTION_N) & 1)
            touched_ns[pos.y][pos.x]++;
        if ((con >> DIRECTION_E) & 1)
            touched_ew[pos.y][pos.x+1]++;
        if ((con >> DIRECTION_S) & 1)
            touched_ns[pos.y+1][pos.x]++;
        if ((con >> DIRECTION_W) & 1)
            touched_ew[pos.y][pos.x]++;

        PressureAdjacent adjl(connections_ns[pos.y][pos.x],
                                 connections_ew[pos.y][pos.x+1],
                                 connections_ns[pos.y+1][pos.x],
                                 connections_ew[pos.y][pos.x]);
        elements[pos.y][pos.x]->sim_prep(adjl, fast_sim);
    }

    fast_sim.add_pipe2(connections_ns[0][4], adj.N);
    fast_sim.add_pipe2(connections_ew[4][9], adj.E);
    fast_sim.add_pipe2(connections_ns[9][4], adj.S);
    fast_sim.add_pipe2(connections_ew[4][0], adj.W);

    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        if (touched_ns[pos.y][pos.x] == 2)
        {
            fast_sim.add_pressure(connections_ns[pos.y][pos.x]);
        }
        else if (touched_ns[pos.y][pos.x] == 1)
        {
            fast_sim.add_pressure_vented(connections_ns[pos.y][pos.x]);
        }
        else if (touched_ns[pos.y][pos.x] == 0)
        {
            connections_ns[pos.y][pos.x].clear();
        }

        if (touched_ew[pos.y][pos.x] == 2)
        {
            fast_sim.add_pressure(connections_ew[pos.y][pos.x]);
        }
        else if (touched_ew[pos.y][pos.x] == 1)
        {
            fast_sim.add_pressure_vented(connections_ew[pos.y][pos.x]);
        }
        else if (touched_ew[pos.y][pos.x] == 0)
        {
            connections_ew[pos.y][pos.x].clear();
        }
    }

    fast_prepped = true;
}

void Circuit::sim_pre(PressureAdjacent adj)
{
    if (!fast_prepped)
    {
    	fast_sim.clear();
        sim_prep(adj, fast_sim);
    }


    fast_sim.sim();
}

void Circuit::remove_circles(LevelSet* level_set, std::set<unsigned> seen)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        unsigned level_index = 0xFFFFFFFF;
        Circuit* subcircuit = elements[pos.y][pos.x]->get_subcircuit(&level_index);
        if (level_index != 0xFFFFFFFF)
        {
            
            if (seen.find(level_index) != seen.end())
            {
                set_element_empty(pos, true);
                const char* name = level_set->levels[level_index]->name;
                signs.push_front(Sign(pos*32+XYPos(16,16), DIRECTION_N, std::string("ERROR ") + name));
            }
            else
            {
                std::set<unsigned> sub_seen(seen);
                sub_seen.insert(level_index);
                level_set->levels[level_index]->circuit->remove_circles(level_set, sub_seen);
            }
        }
        else if (subcircuit)
        {
            subcircuit->remove_circles(level_set, seen);
        }
    }
}


void Circuit::set_element_empty(XYPos pos, bool no_histoyy)
{
    if (is_blocked(pos))
        return;
    if (elements[pos.y][pos.x]->is_empty())
        return;
    if (!no_histoyy)
        ammend();
    else
        fast_prepped = false;
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementEmpty();
}

void Circuit::set_element_pipe(XYPos pos, Connections con)
{
    if (is_blocked(pos))
        return;
    ammend();
    if (elements[pos.y][pos.x]->get_type() == CIRCUIT_ELEMENT_TYPE_PIPE)
    {
        elements[pos.y][pos.x]->extend_pipe(con);
    }
    else
    {
        delete elements[pos.y][pos.x];
        elements[pos.y][pos.x] = new CircuitElementPipe(con);
    }
}

void Circuit::set_element_valve(XYPos pos, Direction direction)
{
    if (is_blocked(pos))
        return;
    ammend();
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementValve(direction);
}

void Circuit::set_element_source(XYPos pos, Direction direction)
{
    if (is_blocked(pos))
        return;
    ammend();
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementSource(direction);
}

void Circuit::set_element_subcircuit(XYPos pos, Direction direction, unsigned level_index, LevelSet* level_set)
{
    if (is_blocked(pos))
        return;
    ammend();
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementSubCircuit(direction, level_index, level_set);
}

void Circuit::add_sign(Sign sign, bool no_undo)
{
    if (!no_undo)
        ammend();
    signs.push_front(sign);
}

void Circuit::move_selected_elements(std::set<XYPos> &selected_elements, Direction d)
{
    XYPos mov(d);
    std::map<XYPos, CircuitElement*> elems;
    if (selected_elements.empty())
        return;
    
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = old + mov;
        if (is_blocked(pos))
            return;
        if (pos.x < 0 || pos.y < 0 || pos.x > 8 || pos.y > 8)
            return;
        if (selected_elements.find(pos) == selected_elements.end() && !elements[pos.y][pos.x]->is_empty())
            return;
    }

    ammend();
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = old + mov;
        elems[pos] = elements[old.y][old.x];
        elements[old.y][old.x] = new CircuitElementEmpty();
    }
    
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = old + mov;
        delete elements[pos.y][pos.x];
        elements[pos.y][pos.x] = elems[pos];
    }
    
    std::set<XYPos> new_sel;
    for (const XYPos& old: selected_elements)
    {
        new_sel.insert(old + mov);
    }
    selected_elements.clear();
    selected_elements = new_sel;
}

void Circuit::delete_selected_elements(std::set<XYPos> &selected_elements)
{
    bool all_empty = true;
    for (const XYPos& pos: selected_elements)
        if (!elements[pos.y][pos.x]->is_empty())
            all_empty = false;
    if (all_empty)
        return;
    ammend();
    for (const XYPos& pos: selected_elements)
    {
        delete elements[pos.y][pos.x];
        elements[pos.y][pos.x] = new CircuitElementEmpty();
    }
}

void Circuit::add_pipe_drag_list(std::list<XYPos> &pipe_drag_list)
{
    ammend();

    XYPos n1,n2,n3;
    std::list<XYPos>::iterator it = pipe_drag_list.begin();
    n2 = *it;
    it++;
    n3 = *it;
    it++;
    for (;it != pipe_drag_list.end(); ++it)
    {
        n1 = n2;
        n2 = n3;
        n3 = *it;
        XYPos d1 = n1 - n2;
        XYPos d2 = n3 - n2;
        XYPos d = d1 + d2;
        
        Connections con = Connections(0);
        
        if (d.y < 0)
        {
            if (d.x < 0)
                con = CONNECTIONS_NW;
            else if (d.x > 0)
                con = CONNECTIONS_NE;
            else
                assert(0);
        }
        else if (d.y > 0)
        {
            if (d.x < 0)
                con = CONNECTIONS_WS;
            else if (d.x > 0)
                con = CONNECTIONS_ES;
            else
                assert(0);
        }
        else
        {
            if (d1.x)
                con = CONNECTIONS_EW;
            else if (d1.y)
                con = CONNECTIONS_NS;
            else assert(0);
        }
        if (elements[n2.y][n2.x]->get_type() == CIRCUIT_ELEMENT_TYPE_PIPE)
        {
            elements[n2.y][n2.x]->extend_pipe(con);
        }
        else
        {
            delete elements[n2.y][n2.x];
            elements[n2.y][n2.x] = new CircuitElementPipe(con);
        }
    }
    
}

void Circuit::force_element(XYPos pos, CircuitElement* element)
{
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = element;
    blocked[pos.y][pos.x] = true;
}

bool Circuit::is_blocked(XYPos pos)
{
    if (pos.x < 0 || pos.y < 0 || pos.x > 8 || pos.y > 8)
        return true;
    return blocked[pos.y][pos.x];
}

void Circuit::ammend()
{
    fast_prepped = false;
    undo_list.push_front(new Circuit(*this));
    for (const Circuit* c: redo_list)
        delete c;
    redo_list.clear();
}

void Circuit::undo(unsigned level_index, LevelSet* level_set)
{
    if (!undo_list.empty())
    {
        fast_prepped = false;
        redo_list.push_front(new Circuit(*this));
        Circuit* to = undo_list.front();
        undo_list.pop_front();
        copy_elements(*to);
        delete to;
        remove_circles(level_set);
        elaborate(level_set);
    }
}

void Circuit::redo(unsigned level_index, LevelSet* level_set)
{
    if (!redo_list.empty())
    {
        fast_prepped = false;
        undo_list.push_front(new Circuit(*this));
        Circuit* to = redo_list.front();
        redo_list.pop_front();
        copy_elements(*to);
        delete to;
        remove_circles(level_set);
        elaborate(level_set);
    }
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
