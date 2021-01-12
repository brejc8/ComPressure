#include "Circuit.h"
#include "GameState.h"

#include "SaveState.h"
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

SDL_Rect CircuitElementPipe::getimage(void)
{
    return SDL_Rect{(connections % 4) * 32, (connections / 4) * 32, 32, 32};
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
            sim_pre_2links(adj.N, adj.S); adj.E.vent(); adj.W.vent(); break;
        case CONNECTIONS_EW:
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
void CircuitElementPipe::sim_post(PressureAdjacent adj)
{
//    if (adj.N.value > 100 * PRESSURE_SCALAR) adj.N.value = 100 * PRESSURE_SCALAR;
//    if (adj.E.value > 100 * PRESSURE_SCALAR) adj.E.value = 100 * PRESSURE_SCALAR;
//    if (adj.S.value > 100 * PRESSURE_SCALAR) adj.S.value = 100 * PRESSURE_SCALAR;
//    if (adj.W.value > 100 * PRESSURE_SCALAR) adj.W.value = 100 * PRESSURE_SCALAR;
//     switch(connections)
//     {
//         case CONNECTIONS_NONE:
//             adj.N.value = 0; adj.E.value = 0; adj.S.value = 0; adj.W.value = 0; break;
//         case CONNECTIONS_NW:
//             adj.E.value = 0; adj.S.value = 0; break;
//         case CONNECTIONS_NE:
//             adj.W.value = 0; adj.S.value = 0; break;
//         case CONNECTIONS_NS:
//             adj.E.value = 0; adj.W.value = 0; break;
//         case CONNECTIONS_EW:
//             adj.N.value = 0; adj.S.value = 0; break;
//         case CONNECTIONS_ES:
//             adj.N.value = 0; adj.W.value = 0; break;
//         case CONNECTIONS_WS:
//             adj.N.value = 0; adj.E.value = 0; break;
//         case CONNECTIONS_NWE:
//             adj.S.value = 0; break;
//         case CONNECTIONS_NES:
//             adj.W.value = 0; break;
//         case CONNECTIONS_NWS:
//             adj.E.value = 0; break;
//         case CONNECTIONS_EWS:
//             adj.N.value = 0; break;
//         case CONNECTIONS_NS_WE:
//         case CONNECTIONS_NW_ES:
//         case CONNECTIONS_NE_WS:
//         case CONNECTIONS_ALL:
//             break;
//         default:
//             assert(0);
//     }

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
    else if (con == CONNECTIONS_NONE) connections = CONNECTIONS_NONE;
    else if (con == CONNECTIONS_NS && connections == CONNECTIONS_EW) connections = CONNECTIONS_NS_WE;
    else if (con == CONNECTIONS_EW && connections == CONNECTIONS_NS) connections = CONNECTIONS_NS_WE;
    else if (con == CONNECTIONS_NW && connections == CONNECTIONS_ES) connections = CONNECTIONS_NW_ES;
    else if (con == CONNECTIONS_ES && connections == CONNECTIONS_NW) connections = CONNECTIONS_NW_ES;
    else if (con == CONNECTIONS_NE && connections == CONNECTIONS_WS) connections = CONNECTIONS_NE_WS;
    else if (con == CONNECTIONS_WS && connections == CONNECTIONS_NE) connections = CONNECTIONS_NE_WS;
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

void CircuitElementValve::reset()
{
    pos_charge = 0;
    neg_charge = 0;
}

SDL_Rect CircuitElementValve::getimage(void)
{
    return SDL_Rect{direction * 32, 4 * 32, 32, 32};
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

void CircuitElementValve::sim_post(PressureAdjacent adj_)
{
}


CircuitElementSource::CircuitElementSource(SaveObjectMap* omap)
{
    direction = Direction(omap->get_num("direction"));
}

void CircuitElementSource::save(SaveObjectMap* omap)
{
    omap->add_num("direction", direction);
}

SDL_Rect CircuitElementSource::getimage(void)
{
    return SDL_Rect{(direction + 4) * 32, 0, 32, 32};
}

void CircuitElementSource::sim_pre(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
    
    adj.N.move((100 * PRESSURE_SCALAR - adj.N.value)/2);
    adj.E.vent();
    adj.S.vent();
    adj.W.vent();
}

void CircuitElementSource::sim_post(PressureAdjacent adj_)
{
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
    reset();
}

Circuit::Circuit()
{
    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        elements[pos.y][pos.x] = new CircuitElementPipe();
    }
    reset();
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

void Circuit::reset(void)
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

}

void Circuit::sim_pre(PressureAdjacent adj)
{
    connections_ns[0][4].value = adj.N.value;
    connections_ew[4][9].value = adj.E.value;
    connections_ns[9][4].value = adj.S.value;
    connections_ew[4][0].value = adj.W.value;

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
        PressureAdjacent adj(connections_ns[pos.y][pos.x],
                             connections_ew[pos.y][pos.x+1],
                             connections_ns[pos.y+1][pos.x],
                             connections_ew[pos.y][pos.x]);
        elements[pos.y][pos.x]->sim_pre(adj);
    }
}

void Circuit::sim_post(PressureAdjacent adj)
{
    connections_ns[0][4].value = adj.N.value;
    connections_ew[4][9].value = adj.E.value;
    connections_ns[9][4].value = adj.S.value;
    connections_ew[4][0].value = adj.W.value;

    XYPos pos;
    for (pos.y = 0; pos.y < 9; pos.y++)
    for (pos.x = 0; pos.x < 9; pos.x++)
    {
        PressureAdjacent adj(connections_ns[pos.y][pos.x],
                             connections_ew[pos.y][pos.x+1],
                             connections_ns[pos.y+1][pos.x],
                             connections_ew[pos.y][pos.x]);
        elements[pos.y][pos.x]->sim_post(adj);
    }

    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        connections_ns[pos.y][pos.x].post();
        connections_ew[pos.y][pos.x].post();
    }



    adj.N.value = connections_ns[0][4].value;
    adj.E.value = connections_ew[4][9].value;
    adj.S.value = connections_ns[9][4].value;
    adj.W.value = connections_ew[4][0].value;

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
}

void Circuit::set_element_valve(XYPos pos, Direction direction)
{
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementValve(direction);
}

void Circuit::set_element_source(XYPos pos, Direction direction)
{
    delete elements[pos.y][pos.x];
    elements[pos.y][pos.x] = new CircuitElementSource(direction);
}
