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

CircuitElement* CircuitElement::load(SaveObject* obj, bool read_only)
{
    if (obj->is_num())
    {
        return new CircuitElementEmpty();
    }
    SaveObjectMap* omap = obj->get_map();
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
            return new CircuitElementSubCircuit(omap, read_only);
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
        case CONNECTIONS_NONE:  return  0;
        case CONNECTIONS_NW:    return  9;
        case CONNECTIONS_NE:    return  3;
        case CONNECTIONS_NS:    return  5;
        case CONNECTIONS_EW:    return 10;
        case CONNECTIONS_ES:    return  6;
        case CONNECTIONS_WS:    return 12;
        case CONNECTIONS_NWE:   return 11;
        case CONNECTIONS_NES:   return  7;
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

void CircuitElementPipe::rotate(bool clockwise)
{
    switch(connections)
    {
        case CONNECTIONS_NONE:
            break;
        case CONNECTIONS_NW:
        case CONNECTIONS_NE:
        case CONNECTIONS_NS:
        case CONNECTIONS_EW:
        case CONNECTIONS_ES:
        case CONNECTIONS_WS:
        case CONNECTIONS_NWE:
        case CONNECTIONS_NES:
        case CONNECTIONS_NWS:
        case CONNECTIONS_EWS:
        {
            unsigned bm = con_to_bitmap(connections);
            if (clockwise)
                bm = (bm << 1) | (bm >> 3);
            else
                bm = (bm >> 1) | (bm << 3);
            bm &= 0xF;
            connections = bitmap_to_con(bm);
            break;
        }
        case CONNECTIONS_NS_WE:
            break;
        case CONNECTIONS_NW_ES:
            connections = CONNECTIONS_NE_WS;
            break;
        case CONNECTIONS_NE_WS:
            connections = CONNECTIONS_NW_ES;
            break;
        case CONNECTIONS_ALL:
            break;
        default:
            assert(0);
    }

}

void CircuitElementPipe::flip(bool vertically)
{
    switch(connections)
    {
        case CONNECTIONS_NONE:
            break;
        case CONNECTIONS_NW:
        case CONNECTIONS_NE:
        case CONNECTIONS_NS:
        case CONNECTIONS_EW:
        case CONNECTIONS_ES:
        case CONNECTIONS_WS:
        case CONNECTIONS_NWE:
        case CONNECTIONS_NES:
        case CONNECTIONS_NWS:
        case CONNECTIONS_EWS:
        {
            unsigned bm = con_to_bitmap(connections);
            unsigned mask = vertically ? 0x5 : 0xA;
            bm = (bm & ~mask) | ((bm & mask) << 2) | ((bm & mask) >> 2);
            bm &= 0xF;
            connections = bitmap_to_con(bm);
            break;
        }
        case CONNECTIONS_NS_WE:
            break;
        case CONNECTIONS_NW_ES:
            connections = CONNECTIONS_NE_WS;
            break;
        case CONNECTIONS_NE_WS:
            connections = CONNECTIONS_NW_ES;
            break;
        case CONNECTIONS_ALL:
            break;
        default:
            assert(0);
    }

}

unsigned CircuitElementPipe::get_cost()
{
    switch(connections)
    {
        case CONNECTIONS_NONE:
            return 0;
        case CONNECTIONS_NW:
        case CONNECTIONS_NE:
        case CONNECTIONS_NS:
        case CONNECTIONS_EW:
        case CONNECTIONS_ES:
        case CONNECTIONS_WS:
            return 2;
        case CONNECTIONS_NWE:
        case CONNECTIONS_NES:
        case CONNECTIONS_NWS:
        case CONNECTIONS_EWS:
            return 3;
        case CONNECTIONS_NS_WE:
        case CONNECTIONS_NW_ES:
        case CONNECTIONS_NE_WS:
        case CONNECTIONS_ALL:
            return 4;
    }
    assert(0);
}


CircuitElementValve::CircuitElementValve(SaveObjectMap* omap)
{
    dir_flip = DirFlip(omap->get_num("direction"));
}

void CircuitElementValve::save(SaveObjectMap* omap)
{
    omap->add_num("direction", dir_flip.as_int());
}

uint16_t CircuitElementValve::get_desc()
{
    return dir_flip.as_int();
}

void CircuitElementValve::reset()
{
}

SDL_Rect CircuitElementValve::getimage_bg(void)
{
    if (dir_flip.dir == DIRECTION_E || dir_flip.dir == DIRECTION_W)
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
    if (dir_flip.dir == DIRECTION_E || dir_flip.dir == DIRECTION_W)
        return XYPos(472 + openness * 24, 240);
    else
        return XYPos(472 + openness * 24, 240 + 24);
}

XYPos CircuitElementValve::getimage(void)
{
    Direction dir = dir_flip.get_n();
    
    return XYPos(int(dir) * 32, 4 * 32);
}

void CircuitElementValve::render_prep(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, dir_flip);
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
    if ((dir_flip.dir == DIRECTION_N) ||
        (dir_flip.dir == DIRECTION_E  && !dir_flip.flp) ||
        (dir_flip.dir == DIRECTION_W  &&  dir_flip.flp))
        move_by = -move_by;
    move_by = std::min(move_by, 100);
    move_by = std::max(move_by, -100);
    
    moved_pos += move_by;
     
}

void CircuitElementValve::sim_prep(PressureAdjacent adj_, FastSim& fast_sim)
{
     PressureAdjacent adj(adj_, dir_flip);
     fast_sim.add_valve(*this, adj);
}
void CircuitElementValve::sim(PressureAdjacent adj)
{
    int64_t mul = (adj.N.value - adj.S.value);
    if (mul < 0)
        mul = 0;

                                                            // base resistence is 8 pipes
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

// SaveObject* CircuitElementEmpty::save()
// {
//     return new SaveObjectNumber(0);
// }

void CircuitElementEmpty::save(SaveObjectMap* omap)
{
}

uint16_t CircuitElementEmpty::get_desc()
{
    return 0;
}

XYPos CircuitElementEmpty::getimage(void)
{
    return XYPos(-1, -1);
}

CircuitElementSubCircuit::~CircuitElementSubCircuit()
{
    if (circuit)
        delete circuit;
}

CircuitElementSubCircuit::CircuitElementSubCircuit(DirFlip dir_flip_, int level_index_, LevelSet* level_set, bool read_only_):
    dir_flip(dir_flip_),
    level_index(level_index_),
    read_only(read_only_)
{
    level = level_set->levels[level_index];
    name = level->name;
    circuit = NULL;
    elaborate(level_set);
}

CircuitElementSubCircuit::CircuitElementSubCircuit(SaveObjectMap* omap, bool read_only_):
    read_only(read_only_)
{
    dir_flip = Direction(omap->get_num("direction"));
    level_index = Direction(omap->get_num("level_index"));
    level = NULL;
    circuit = NULL;
    if (omap->has_key("read_only"))
        read_only = true;
    if (omap->has_key("circuit"))
    {
        circuit = new Circuit(omap->get_item("circuit")->get_map());
        custom = true;
    }
}

CircuitElementSubCircuit::CircuitElementSubCircuit(CircuitElementSubCircuit& other)
{
    dir_flip = other.dir_flip;
    level_index = other.level_index;
    level = other.level;
    custom  = other.custom;
    name = other.name;
    if (other.circuit)
    {
        circuit = new Circuit(*other.circuit);
    }
}

void CircuitElementSubCircuit::save(SaveObjectMap* omap)
{
    omap->add_num("direction", dir_flip.as_int());
    omap->add_num("level_index", level_index);
    if (custom)
    {
        omap->add_item("circuit", circuit->save());
    }
}

uint16_t CircuitElementSubCircuit::get_desc()
{
    return (level_index << 4) | dir_flip.as_int() << 1 | custom;
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
    level_index = level_set->find_level(level_index, name);
    if (level_index >= 0)
        level = level_set->levels[level_index];
    else
    {
        custom = true;
        level = NULL;
    }

    if (level_index >= LEVEL_COUNT)
        name = level->name;
    if (!custom)
    {
        assert (level_index >= 0);
        if (circuit)
            delete circuit;
//        level->circuit->remove_circles(level_set);
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

bool CircuitElementSubCircuit::contains_subcircuit_level(int level_index_q, LevelSet* level_set)
{
    if (!custom && level_index_q == level_index)
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
    
    return dir_flip.mask(con);
}

SDL_Rect CircuitElementSubCircuit::getimage_bg(void)
{
    return SDL_Rect{custom ? 232 : 208, 160, 24, 24};
}

XYPos CircuitElementSubCircuit::getimage(void)
{
    if (level_index < 0)
        return XYPos(128 + 3 * 32, 32 + 3 * 32);
    assert(level);
//    return level->getimage(dir_flip);
    int mask = getconnections();
    return XYPos(128 + (mask & 0x3) * 32, 32 + ((mask >> 2) & 0x3) * 32);
}

XYPos CircuitElementSubCircuit::getimage_fg(void)
{
    if (level_index < 0)
        return XYPos(dir_flip.as_int() * 24 + 192, 944);
    assert(level);
    return level->getimage_fg(dir_flip);
}

WrappedTexture* CircuitElementSubCircuit::getimage_fg_texture(void)
{
    if (level_index < 0)
        return NULL;
    assert(level);
    return level->getimage_fg_texture();
}

void CircuitElementSubCircuit::sim_prep(PressureAdjacent adj_, FastSim& fast_sim)
{
    static CircuitPressure def;
    PressureAdjacent adj(PressureAdjacent(adj_, getconnections(), def), dir_flip);

    assert(circuit);
    circuit->sim_prep(adj, fast_sim);
}

unsigned CircuitElementSubCircuit::get_cost()
{
    return 4 + circuit->get_cost();
}
void CircuitElementSubCircuit::reindex_deleted_level(LevelSet* level_set, int deleted_level_index)
{
    if (level_index > deleted_level_index)
        level_index--;
    else if (level_index == deleted_level_index)
    {
        if (!custom)
        {
            custom = true;
            if (circuit)
                delete circuit;
            circuit = new Circuit(*level->circuit);
            level = NULL;
            level_index = -1;
        }
    }
    if (custom)
        circuit->reindex_deleted_level(level_set, deleted_level_index);
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
    direction = direction_rotate(direction, clockwise);
}

void Sign::flip(bool vertically)
{
    direction = direction_flip(direction, vertically);
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
            if (pos.x < slist_x->get_count())
                elements[pos.y][pos.x] = CircuitElement::load(slist_x->get_item(pos.x));
            else
                elements[pos.y][pos.x] = new CircuitElementEmpty();
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
    signs = other.signs;
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
            int x = pos.x;
            while (true)
            {
                if (!is_blocked(XYPos(x, pos.y)) && !elements[pos.y][x]->is_empty())
                    break;
                x++;
                if (x >= 9)
                    break;
            }
            if (x >= 9)
                break;
            if (is_blocked(pos))
            {
                CircuitElementEmpty tmp;
                slist_x->add_item(((CircuitElement*)(&tmp))->save());
            }
            else
            {
                slist_x->add_item(elements[pos.y][pos.x]->save());
            }
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
        if (is_blocked(pos))
            continue;

        if (elements[pos.y][pos.x]->get_custom()
         || elements[pos.y][pos.x]->get_type() != other.elements[pos.y][pos.x]->get_type()
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
    fast_prepped = false;
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

void Circuit::prep(PressureAdjacent adj)
{
    if (!fast_prepped)
    {
    	fast_sim.clear();
        sim_prep(adj, fast_sim);
    }
}

void Circuit::sim_pre(PressureAdjacent adj)
{
    fast_sim.sim();
}

void Circuit::remove_circles(LevelSet* level_set, std::set<unsigned> seen)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        int level_index = 0xFFFFFFFF;
        Circuit* subcircuit = elements[pos.y][pos.x]->get_subcircuit(&level_index);
        if (level_index != 0xFFFFFFFF)
        {
            if (elements[pos.y][pos.x]->get_custom())
            {
                subcircuit->remove_circles(level_set, seen);
            }
            else
            {
                if (seen.find(level_index) != seen.end())
                {
                    set_element_empty(pos, true);
                    signs.push_front(Sign(pos*32+XYPos(16,16), DIRECTION_N, std::string("ERROR ") + level_set->levels[level_index]->name));
                }
                else if (!level_set->is_playable(level_index, 1000000))
                {
                    set_element_empty(pos, true);
                    signs.push_front(Sign(pos*32+XYPos(16,16), DIRECTION_N, std::string("ERROR")));
                }
                else
                {
                    std::set<unsigned> sub_seen(seen);
                    sub_seen.insert(level_index);
                    level_set->levels[level_index]->circuit->remove_circles(level_set, sub_seen);
                }
            }
        }
    }
}

void Circuit::remove_sign(std::list<Sign>::iterator it, bool no_history)
{
    if (!no_history)
        ammend();
    else
        fast_prepped = false;
    signs.erase(it);
}


void Circuit::set_element_empty(XYPos pos, bool no_history)
{
    if (is_blocked(pos))
        return;
    if (elements[pos.y][pos.x]->is_empty())
        return;
    if (!no_history)
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

void Circuit::set_element_valve(XYPos pos, DirFlip dir_flip)
{
    if (is_blocked(pos))
        return;
    ammend();
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementValve(dir_flip);
}

void Circuit::set_element_source(XYPos pos, Direction direction)
{
    if (is_blocked(pos))
        return;
    ammend();
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementSource(direction);
}

void Circuit::set_element_subcircuit(XYPos pos, DirFlip dir_flip, int level_index, LevelSet* level_set)
{
    if (is_blocked(pos))
        return;
    ammend();
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementSubCircuit(dir_flip, level_index, level_set);
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
        if (is_blocked(old))
            return;
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

void Circuit::rotate_selected_elements(std::set<XYPos> &selected_elements, bool clockwise)
{
    std::map<XYPos, CircuitElement*> elems;
    if (selected_elements.empty())
        return;
    
    XYPos min = XYPos(9,9);
    XYPos max = XYPos(0,0);
    
    for (const XYPos& old: selected_elements)
    {
        min.x = std::min(min.x, old.x);
        min.y = std::min(min.y, old.y);
        max.x = std::max(max.x, old.x);
        max.y = std::max(max.y, old.y);
    }
    XYPos center = (min + max) / 2;
    
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = ((old - min) * (clockwise ? DIRECTION_E : DIRECTION_W) + XYPos(clockwise ? max.x : min.x, clockwise ? min.y : max.y));
        if (is_blocked(old))
            return;
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
        XYPos pos = ((old - min) * (clockwise ? DIRECTION_E : DIRECTION_W) + XYPos(clockwise ? max.x : min.x, clockwise ? min.y : max.y));
        elems[pos] = elements[old.y][old.x];
        elements[old.y][old.x] = new CircuitElementEmpty();
    }
    
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = ((old - min) * (clockwise ? DIRECTION_E : DIRECTION_W) + XYPos(clockwise ? max.x : min.x, clockwise ? min.y : max.y));
        delete elements[pos.y][pos.x];
        elements[pos.y][pos.x] = elems[pos];
        elements[pos.y][pos.x]->rotate(clockwise);
    }
    
    std::set<XYPos> new_sel;
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = ((old - min) * (clockwise ? DIRECTION_E : DIRECTION_W) + XYPos(clockwise ? max.x : min.x, clockwise ? min.y : max.y));
        new_sel.insert(pos);
    }
    selected_elements.clear();
    selected_elements = new_sel;
}

void Circuit::flip_selected_elements(std::set<XYPos> &selected_elements, bool vertically)
{
    std::map<XYPos, CircuitElement*> elems;
    if (selected_elements.empty())
        return;
    
    XYPos min = XYPos(9,9);
    XYPos max = XYPos(0,0);
    
    for (const XYPos& old: selected_elements)
    {
        min.x = std::min(min.x, old.x);
        min.y = std::min(min.y, old.y);
        max.x = std::max(max.x, old.x);
        max.y = std::max(max.y, old.y);
    }
    
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = XYPos(vertically ? old.x : max.x - old.x + min.x, vertically ? max.y - old.y + min.y : old.y);
        if (is_blocked(old))
            return;
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
        XYPos pos = XYPos(vertically ? old.x : max.x - old.x + min.x, vertically ? max.y - old.y + min.y : old.y);
        elems[pos] = elements[old.y][old.x];
        elements[old.y][old.x] = new CircuitElementEmpty();
    }
    
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = XYPos(vertically ? old.x : max.x - old.x + min.x, vertically ? max.y - old.y + min.y : old.y);
        delete elements[pos.y][pos.x];
        elements[pos.y][pos.x] = elems[pos];
        elements[pos.y][pos.x]->flip(vertically);
    }
    
    std::set<XYPos> new_sel;
    for (const XYPos& old: selected_elements)
    {
        XYPos pos = XYPos(vertically ? old.x : max.x - old.x + min.x, vertically ? max.y - old.y + min.y : old.y);
        new_sel.insert(pos);
    }
    
    selected_elements.clear();
    selected_elements = new_sel;
}

void Circuit::delete_selected_elements(std::set<XYPos> &selected_elements)
{
    bool all_empty = true;
    for (const XYPos& pos: selected_elements)
    {
        if (!elements[pos.y][pos.x]->is_empty() && !is_blocked(pos))
            all_empty = false;
    }
    if (all_empty)
        return;

    ammend();
    for (const XYPos& pos: selected_elements)
    {
        if (is_blocked(pos))
            continue;
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
    elements[pos.y][pos.x]->set_read_only(true);
}

void Circuit::force_sign(Sign new_sign)
{
    for (Sign &sign : signs)
    {
        if (new_sign.text == sign.text)
            return;
    }
    signs.push_back(new_sign);
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

void Circuit::undo(LevelSet* level_set)
{
    if (!undo_list.empty())
    {
        redo_list.push_front(new Circuit(*this));
        Circuit* to = undo_list.front();
        undo_list.pop_front();
        copy_elements(*to);
        delete to;
        remove_circles(level_set);
        elaborate(level_set);
    }
}

void Circuit::redo(LevelSet* level_set)
{
    if (!redo_list.empty())
    {
        undo_list.push_front(new Circuit(*this));
        Circuit* to = redo_list.front();
        redo_list.pop_front();
        copy_elements(*to);
        delete to;
        remove_circles(level_set);
        elaborate(level_set);
    }
}

void Circuit::paste(Clipboard& clipboard, XYPos offset, LevelSet* level_set)
{
    ammend();
    for (Clipboard::ClipboardElement& clip_elem: clipboard.elements)
    {
        XYPos pos = clip_elem.pos + offset;
        CircuitElement* element = clip_elem.element;

        if (is_blocked(pos))
            continue;
        
        if (elements[pos.y][pos.x]->get_custom()
         || elements[pos.y][pos.x]->get_type() != element->get_type()
         || elements[pos.y][pos.x]->get_desc() != element->get_desc())
        {
            delete elements[pos.y][pos.x];
            elements[pos.y][pos.x] = element->copy();
        }
    }
    remove_circles(level_set);
    elaborate(level_set);
}

bool Circuit::contains_subcircuit_level(int level_index, LevelSet* level_set)
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

unsigned Circuit::get_cost()
{
    unsigned cost = 0;
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        cost += elements[pos.y][pos.x]->get_cost();
    }
    return cost;
}

SaveObjectList* Circuit::save_forced()
{
    SaveObjectList* slist = new SaveObjectList;
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        if (is_blocked(pos))
        {
            SaveObjectMap* omap = new SaveObjectMap;
            omap->add_num("x", pos.x);
            omap->add_num("y", pos.y);
            omap->add_item("element", elements[pos.y][pos.x]->save());
            slist->add_item(omap);

        }
    }
    return slist;
}

void Circuit::copy_in(Circuit* other)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        delete elements[pos.y][pos.x];
        elements[pos.y][pos.x] = other->elements[pos.y][pos.x]->copy();
        blocked[pos.y][pos.x] = other->blocked[pos.y][pos.x];
    }
    signs = other->signs;
}

void Circuit::reindex_deleted_level(LevelSet* level_set, int level_index)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x]->reindex_deleted_level(level_set, level_index);
    }
}

void Clipboard::copy(std::set<XYPos> &selected_elements, Circuit &circuit)
{
    for (const ClipboardElement& elem: elements)
    {
        delete elem.element;
    }
    elements.clear();

    for (const XYPos& pos: selected_elements)
    {
        elements.push_back(ClipboardElement(pos, circuit.elements[pos.y][pos.x]->copy()));
    }
    repos();
}

void Clipboard::repos()
{
    if (elements.empty())
        return;
    XYPos pos = elements.front().pos;
    for (ClipboardElement& elem: elements)
    {
        pos.x = std::min(pos.x, elem.pos.x);
        pos.y = std::min(pos.y, elem.pos.y);
    }
    for (ClipboardElement& elem: elements)
    {
        elem.pos -= pos;
    }
}

XYPos Clipboard::size()
{
    XYPos pos(0,0);
    for (ClipboardElement& elem: elements)
    {
        pos.x = std::max(pos.x, elem.pos.x);
        pos.y = std::max(pos.y, elem.pos.y);
    }
    pos += XYPos(1, 1);
    return pos;
}

void Clipboard::rotate(bool clockwise)
{
    repos();
    for (ClipboardElement& elem: elements)
    {
        int oldx = elem.pos.x;
        elem.pos.x = clockwise ? -elem.pos.y : elem.pos.y;
        elem.pos.y = clockwise ? oldx : -oldx;
        elem.element->rotate(clockwise);
    }
    repos();
}

void Clipboard::flip(bool vertically)
{
    repos();
    for (ClipboardElement& elem: elements)
    {
        if (vertically)
            elem.pos.y = -elem.pos.y;
        else
            elem.pos.x = -elem.pos.x;
        elem.element->flip(vertically);
    }
    repos();
}

void Clipboard::elaborate(LevelSet* level_set)
{
    for (ClipboardElement& elem: elements)
    {
        elem.element->elaborate(level_set);
    }
}
