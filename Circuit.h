#pragma once
#include "Misc.h"
#include <SDL.h>

typedef int Pressure;

enum Connections
{
    CONNECTIONS_NONE,
    CONNECTIONS_NW,
    CONNECTIONS_NE,
    CONNECTIONS_NS,
    CONNECTIONS_EW,
    CONNECTIONS_ES,
    CONNECTIONS_WS,
    CONNECTIONS_NWE,
    CONNECTIONS_NES,
    CONNECTIONS_NWS,
    CONNECTIONS_EWS,
    CONNECTIONS_NS_WE,
    CONNECTIONS_NW_ES,
    CONNECTIONS_NE_WS,
    CONNECTIONS_ALL,
};

enum CircuitElementType
{
    CIRCUIT_ELEMENT_TYPE_PIPE,
    CIRCUIT_ELEMENT_TYPE_VALVE,
};
class PressureAdjacent
{
public:
    Pressure& N;
    Pressure& E;
    Pressure& S;
    Pressure& W;
    PressureAdjacent(Pressure& N_, Pressure& E_, Pressure& S_, Pressure& W_):
    N(N_),
    E(E_),
    S(S_),
    W(W_)
    {}

    PressureAdjacent(PressureAdjacent adj, Direction dir):
        N(dir == DIRECTION_N ? adj.N : dir == DIRECTION_E ? adj.E : dir == DIRECTION_S ? adj.S : adj.W),
        E(dir == DIRECTION_N ? adj.E : dir == DIRECTION_E ? adj.S : dir == DIRECTION_S ? adj.W : adj.N),
        S(dir == DIRECTION_N ? adj.S : dir == DIRECTION_E ? adj.W : dir == DIRECTION_S ? adj.N : adj.E),
        W(dir == DIRECTION_N ? adj.W : dir == DIRECTION_E ? adj.N : dir == DIRECTION_S ? adj.E : adj.S)
    {
    }
};

class CircuitElement
{
public:
    virtual SDL_Rect getimage(void) = 0;
    virtual void sim_pre(PressureAdjacent adj) = 0;
    virtual void sim_post(PressureAdjacent adj) = 0;
    
    virtual CircuitElementType get_type() = 0;
    virtual void extend_pipe(Connections con){assert(0);}
};

class CircuitElementPipe : public CircuitElement
{
public:
    Connections connections = CONNECTIONS_NONE;
    Pressure move_N;
    Pressure move_E;
    Pressure move_S;
    Pressure move_W;

    CircuitElementPipe(){}
    CircuitElementPipe(Connections connections_):
        connections(connections_)
        {}

    SDL_Rect getimage(void);
    void sim_pre(PressureAdjacent adj);
    void sim_post(PressureAdjacent adj);
    CircuitElementType get_type() {return CIRCUIT_ELEMENT_TYPE_PIPE;}
    void extend_pipe(Connections con);
};



class CircuitElementValve : public CircuitElement
{
    const int capacity = 100;
public:
    Direction direction = DIRECTION_N;

    CircuitElementValve(){}
    CircuitElementValve(Direction direction_):
        direction(direction_)
        {}

    SDL_Rect getimage(void);
    void sim_pre(PressureAdjacent adj);
    void sim_post(PressureAdjacent adj);
    CircuitElementType get_type() {return CIRCUIT_ELEMENT_TYPE_VALVE;}
};



class Circuit
{
public:
    CircuitElement* elements[9][9];

    Pressure connections_ns[10][10];
    Pressure connections_ew[10][10];
    Circuit();
    
    void set_element_pipe(XYPos pos, Connections con);
    void set_element_valve(XYPos pos, Direction direction);

    void reset(void);
    void sim_pre(PressureAdjacent);
    void sim_post(PressureAdjacent);

};
