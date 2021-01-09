#include "Circuit.h"
#include "GameState.h"

#include "SaveState.h"
#include "Misc.h"

SDL_Rect CircuitElementPipe::getimage(void)
{
    return SDL_Rect{(connections % 4) * 32, (connections / 4) * 32, 32, 32};
}

static void sim_pre_2links(Pressure& a, Pressure& b, Pressure& move_a, Pressure& move_b)
{
    int mov = (a - b) / 2;
    move_a = -mov;
    move_b = mov;
}

static void sim_pre_3links(Pressure& a, Pressure& b, Pressure& c, Pressure& move_a, Pressure& move_b, Pressure& move_c)
{
    int mov = (a - b) / 3;
    move_a = -mov;
    move_b = mov;

    mov = (a - c) / 3;
    move_a += -mov;
    move_c = mov;

    mov = (b - c) / 3;
    move_b += -mov;
    move_c += mov;
}

static void sim_pre_4links(Pressure& a, Pressure& b, Pressure& c, Pressure& d, Pressure& move_a, Pressure& move_b, Pressure& move_c, Pressure& move_d)
{
    int mov = (a - b) / 5;
    move_a = -mov;
    move_b = mov;

    mov = (a - c) / 5;
    move_a += -mov;
    move_c = mov;

    mov = (b - c) / 5;
    move_b += -mov;
    move_c += mov;

    mov = (a - d) / 5;
    move_a += -mov;
    move_d = mov;

    mov = (b - d) / 5;
    move_b += -mov;
    move_d += mov;

    mov = (c - d) / 5;
    move_c += -mov;
    move_d += mov;
}

void CircuitElementPipe::sim_pre(PressureAdjacent adj)
{
    switch(connections)
    {
        case CONNECTIONS_NONE:
            move_N = 0; move_E = 0; move_S = 0; move_W = 0; break;
        case CONNECTIONS_NW:
            sim_pre_2links(adj.N, adj.W, move_N, move_W); move_E = 0; move_S = 0; break;
        case CONNECTIONS_NE:
            sim_pre_2links(adj.N, adj.E, move_N, move_E); move_W = 0; move_S = 0; break;
        case CONNECTIONS_NS:
            sim_pre_2links(adj.N, adj.S, move_N, move_S); move_E = 0; move_W = 0; break;
        case CONNECTIONS_EW:
            sim_pre_2links(adj.E, adj.W, move_E, move_W); move_N = 0; move_S = 0; break;
        case CONNECTIONS_ES:
            sim_pre_2links(adj.E, adj.S, move_E, move_S); move_N = 0; move_W = 0; break;
        case CONNECTIONS_WS:
            sim_pre_2links(adj.W, adj.S, move_W, move_S); move_N = 0; move_E = 0; break;
        case CONNECTIONS_NWE:
            sim_pre_3links(adj.N, adj.W, adj.E, move_N, move_W, move_E); move_S = 0; break;
        case CONNECTIONS_NES:
            sim_pre_3links(adj.N, adj.E, adj.S, move_N, move_E, move_S); move_W = 0; break;
        case CONNECTIONS_NWS:
            sim_pre_3links(adj.N, adj.W, adj.S, move_N, move_W, move_S); move_E = 0; break;
        case CONNECTIONS_EWS:
            sim_pre_3links(adj.E, adj.W, adj.S, move_E, move_W, move_S); move_N = 0; break;
        case CONNECTIONS_NS_WE:sim_pre_2links(adj.N, adj.S, move_N, move_S); sim_pre_2links(adj.E, adj.W, move_E, move_W); break;
        case CONNECTIONS_NW_ES:sim_pre_2links(adj.N, adj.W, move_N, move_W); sim_pre_2links(adj.E, adj.S, move_E, move_S); break;
        case CONNECTIONS_NE_WS:sim_pre_2links(adj.N, adj.E, move_N, move_E); sim_pre_2links(adj.W, adj.S, move_W, move_S); break;
        case CONNECTIONS_ALL:
            sim_pre_4links(adj.N, adj.E, adj.W, adj.S, move_N, move_E, move_W, move_S); break;
        default:
            assert(0);
    }
}

void CircuitElementPipe::sim_post(PressureAdjacent adj)
{
    adj.N += move_N;
    adj.E += move_E;
    adj.S += move_S;
    adj.W += move_W;
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


SDL_Rect CircuitElementValve::getimage(void)
{
    return SDL_Rect{direction * 32, 4 * 32, 32, 32};
}

void CircuitElementValve::sim_pre(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
}

void CircuitElementValve::sim_post(PressureAdjacent adj_)
{
    PressureAdjacent adj(adj_, direction);
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

void Circuit::reset(void)
{
    XYPos pos;
    for (pos.y = 0; pos.y < 10; pos.y++)
    for (pos.x = 0; pos.x < 10; pos.x++)
    {
        connections_ns[pos.y][pos.x] = 0;
        connections_ew[pos.y][pos.x] = 0;
    }
}

void Circuit::sim_pre(PressureAdjacent adj)
{
    connections_ns[0][4] = adj.N;
    connections_ew[4][9] = adj.E;
    connections_ns[9][4] = adj.S;
    connections_ew[4][0] = adj.W;
    
    XYPos pos;
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
    connections_ns[0][4] = adj.N;
    connections_ew[4][9] = adj.E;
    connections_ns[9][4] = adj.S;
    connections_ew[4][0] = adj.W;

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
    adj.N = connections_ns[0][4];
    adj.E = connections_ew[4][9];
    adj.S = connections_ns[9][4];
    adj.W = connections_ew[4][0];

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
