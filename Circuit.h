#pragma once
#include "Misc.h"
#include "SaveState.h"

#include <vector>
#include <set>
#include <list>
#include <string>
#include <SDL.h>

#define PRESSURE_SCALAR (65536)

class LevelSet;
class Level;

class Circuit;
class Clipboard;
class LevelTexture;
class WrappedTexture;

typedef int Pressure;

static unsigned pressure_as_percent(Pressure p)
{
    return std::min(std::max((p + PRESSURE_SCALAR / 2) / PRESSURE_SCALAR, Pressure(0)), Pressure(100));
}

static double pressure_as_percent_float(Pressure p)
{
    return std::min(std::max(float(p) / PRESSURE_SCALAR, float(Pressure(0))), float(Pressure(100)));
}


static Pressure percent_as_pressure(unsigned p)
{
    return p * PRESSURE_SCALAR;
}


class CircuitPressure
{
public:
    Pressure value = 0;
    Pressure move_next = 0;

    void clear()
    {
        value = 0;
        move_next = 0;
    }
    void apply(Pressure vol, Pressure drive)
    {
        move_next += ((int64_t(vol * PRESSURE_SCALAR - value) * drive) / 100) / 2;
    }


    void move(Pressure vol)
    {
        move_next += vol;
    }
    
    void vent(void)
    {
        move_next -= value / 2;
    }

    void pre(void)
    {
    }
    
    void post(void)
    {
        value += move_next;
        move_next = 0;
    }
    
    CircuitPressure(Pressure value_):
        value(value_)
    {}
        CircuitPressure()
    {}

};

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
    CIRCUIT_ELEMENT_TYPE_SOURCE,
    CIRCUIT_ELEMENT_TYPE_SUBCIRCUIT,
    CIRCUIT_ELEMENT_TYPE_EMPTY
};

class PressureAdjacent
{
public:
    CircuitPressure& N;
    CircuitPressure& E;
    CircuitPressure& S;
    CircuitPressure& W;
    PressureAdjacent(CircuitPressure& N_, CircuitPressure& E_, CircuitPressure& S_, CircuitPressure& W_):
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

    PressureAdjacent(PressureAdjacent adj, DirFlip dir_flip):
        N(dir_flip.get_dir(DIRECTION_N) == DIRECTION_N ? adj.N : dir_flip.get_dir(DIRECTION_N) == DIRECTION_E ? adj.E : dir_flip.get_dir(DIRECTION_N) == DIRECTION_S ? adj.S : adj.W),
        E(dir_flip.get_dir(DIRECTION_E) == DIRECTION_N ? adj.N : dir_flip.get_dir(DIRECTION_E) == DIRECTION_E ? adj.E : dir_flip.get_dir(DIRECTION_E) == DIRECTION_S ? adj.S : adj.W),
        S(dir_flip.get_dir(DIRECTION_S) == DIRECTION_N ? adj.N : dir_flip.get_dir(DIRECTION_S) == DIRECTION_E ? adj.E : dir_flip.get_dir(DIRECTION_S) == DIRECTION_S ? adj.S : adj.W),
        W(dir_flip.get_dir(DIRECTION_W) == DIRECTION_N ? adj.N : dir_flip.get_dir(DIRECTION_W) == DIRECTION_E ? adj.E : dir_flip.get_dir(DIRECTION_W) == DIRECTION_S ? adj.S : adj.W)
    {
    }

    PressureAdjacent(PressureAdjacent adj, unsigned con_mask, CircuitPressure& def):
        N(((con_mask >>  DIRECTION_N) & 1) ? adj.N : def),
        E(((con_mask >>  DIRECTION_E) & 1) ? adj.E : def),
        S(((con_mask >>  DIRECTION_S) & 1) ? adj.S : def),
        W(((con_mask >>  DIRECTION_W) & 1) ? adj.W : def)
    {
    }

};

class CircuitElementValve;

class FastSim
{
    class FastSimPipe2
    {
        CircuitPressure& a;
        CircuitPressure& b;
    public:
        FastSimPipe2(CircuitPressure& a_, CircuitPressure& b_):
            a(a_),
            b(b_)
        {}
        void sim()
        {
            Pressure mov = (a.value - b.value) / 2;
            a.move(-mov);
            b.move(mov);
        }
    };

    class FastSimPipe3
    {
        CircuitPressure& a;
        CircuitPressure& b;
        CircuitPressure& c;
    public:
        FastSimPipe3(CircuitPressure& a_, CircuitPressure& b_, CircuitPressure& c_):
            a(a_),
            b(b_),
            c(c_)
        {}
        void sim()
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
    };

    class FastSimPipe4
    {
        CircuitPressure& a;
        CircuitPressure& b;
        CircuitPressure& c;
        CircuitPressure& d;
    public:
        FastSimPipe4(CircuitPressure& a_, CircuitPressure& b_, CircuitPressure& c_, CircuitPressure& d_):
            a(a_),
            b(b_),
            c(c_),
            d(d_)
        {}
        void sim()
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
    };

    class FastSimValve
    {
        PressureAdjacent adj;
        CircuitElementValve& valve;
    public:
        FastSimValve(CircuitElementValve& valve_, PressureAdjacent adj_):
            valve(valve_),
            adj(adj_)
        {}
        void sim();
    };
    
    std::vector<FastSimPipe2> pipe2;
    std::vector<FastSimPipe3> pipe3;
    std::vector<FastSimPipe4> pipe4;
    std::vector<FastSimValve> valves;
    std::vector<CircuitPressure*> sources;
    std::vector<CircuitPressure*> fast_pressures;
    std::vector<CircuitPressure*> fast_pressures_vent;
    int64_t steam_used;

public:
    void clear()
    {
        pipe2.clear();
        pipe3.clear();
        pipe4.clear();
        valves.clear();
        sources.clear();
        fast_pressures.clear();
        fast_pressures_vent.clear();
   }
    void add_pipe2(CircuitPressure& a, CircuitPressure& b)
    {
        pipe2.push_back(FastSimPipe2(a, b));
    }
    void add_pipe3(CircuitPressure& a, CircuitPressure& b, CircuitPressure& c)
    {
        pipe3.push_back(FastSimPipe3(a, b, c));
    }
    void add_pipe4(CircuitPressure& a, CircuitPressure& b, CircuitPressure& c, CircuitPressure& d)
    {
        pipe4.push_back(FastSimPipe4(a, b, c, d));
    }
    void add_valve(CircuitElementValve& valve, PressureAdjacent adj);
    void add_source(CircuitPressure& a)
    {
        sources.push_back(&a);
    }
    void add_pressure(CircuitPressure& pres)
    {
        fast_pressures.push_back(&pres);
    }
    void add_pressure_vented(CircuitPressure& pres)
    {
        fast_pressures_vent.push_back(&pres);
    }
    
    void sim()
    {
        for (CircuitPressure* con : fast_pressures)
            con->pre();
        for (CircuitPressure* con : fast_pressures_vent)
        {
            con->pre();
            con->vent();
        }

        for (FastSimPipe2& p : pipe2)
            p.sim();
        for (FastSimPipe3& p : pipe3)
            p.sim();
        for (FastSimPipe4& p : pipe4)
            p.sim();
        for (FastSimValve& p : valves)
            p.sim();
        for (CircuitPressure* con : sources)
        {
            int64_t val = (100 * PRESSURE_SCALAR - con->value) / 2;
            steam_used += val;
            con->move(val);
        }

        for (CircuitPressure* con : fast_pressures)
            con->post();
        for (CircuitPressure* con : fast_pressures_vent)
            con->post();
    }
    void reset_steam_used() {steam_used = 0;}
    int64_t get_steam_used() {return (steam_used + PRESSURE_SCALAR / 2) / PRESSURE_SCALAR;}
};


class CircuitElement
{
public:
    SaveObject* save(void);
    virtual void save(SaveObjectMap*) = 0;
    static CircuitElement* load(SaveObject*, bool read_only = false);
    virtual CircuitElement* copy() = 0;
    virtual ~CircuitElement(){}


    virtual uint16_t get_desc() = 0;
    virtual void elaborate(LevelSet* level_set) {}
    virtual void reset() {}
    virtual void retire() {}
    virtual bool contains_subcircuit_level(int level_index, LevelSet* level_set) {return false;}
    virtual unsigned getconnections(void) = 0;
    virtual void render_prep(PressureAdjacent adj) {}
    virtual void sim_prep(PressureAdjacent adj, FastSim& fast_sim) = 0;
    virtual XYPos getimage(void) = 0;
    virtual XYPos getimage_fg(void) {return XYPos(-1,-1);}
    virtual WrappedTexture* getimage_fg_texture() {return NULL;}
    virtual SDL_Rect getimage_bg(void) {return SDL_Rect{0, 0, 0, 0};}
    virtual bool is_empty() {return false;}
    virtual Pressure get_moved(PressureAdjacent adj) {return 0;}

    virtual CircuitElementType get_type() = 0;
    virtual void extend_pipe(Connections con){assert(0);}
    virtual Circuit* get_subcircuit(int *level_index_ = NULL) {return NULL;}
    virtual bool get_custom() {return false;}
    virtual void set_custom(bool recurse = false) {}
    virtual bool get_read_only() {return true;}
    virtual void set_read_only(bool read_only_) {}
    virtual void rotate(bool clockwise) = 0;
    virtual void flip(bool vertically) = 0;
    virtual unsigned get_cost() = 0;
    virtual void reindex_deleted_level(LevelSet* level_set, int level_index) {};
};

class CircuitElementPipe : public CircuitElement
{
public:
    Connections connections = CONNECTIONS_NONE;
    Pressure pressure = 0;
    int moved_pos = 0;

    CircuitElementPipe(){}
    CircuitElementPipe(Connections connections_):
        connections(connections_)
        {}
    CircuitElementPipe(SaveObjectMap*);

    void save(SaveObjectMap*);
    virtual uint16_t get_desc();
    virtual CircuitElement* copy() { return new CircuitElementPipe(connections);}
    unsigned getconnections(void);
    void render_prep(PressureAdjacent adj);
    void sim_prep(PressureAdjacent adj, FastSim& fast_sim);
    XYPos getimage(void);
    SDL_Rect getimage_bg(void);
    virtual Pressure get_moved(PressureAdjacent adj);
    CircuitElementType get_type() {return CIRCUIT_ELEMENT_TYPE_PIPE;}
    void rotate(bool clockwise);
    void flip(bool vertically);
    unsigned get_cost();

    void extend_pipe(Connections con);
};

class CircuitElementValve : public CircuitElement
{
    const int resistence = 8;

    Pressure pressure = 0;
    int openness = 0;
    int moved_pos = 0;

public:
    DirFlip dir_flip;

    CircuitElementValve(){}
    CircuitElementValve(DirFlip dir_flip_):
        dir_flip(dir_flip_)
        {}
    CircuitElementValve(SaveObjectMap*);

    void save(SaveObjectMap*);
    virtual uint16_t get_desc();
    virtual CircuitElement* copy() { return new CircuitElementValve(dir_flip);}
    void reset();
    unsigned getconnections(void) {return 0xF;};
    XYPos getimage(void);
    SDL_Rect getimage_bg(void);
    XYPos getimage_fg(void);
    void render_prep(PressureAdjacent adj);

    void sim_prep(PressureAdjacent adj, FastSim& fast_sim);
    void sim(PressureAdjacent adj);
    CircuitElementType get_type() {return CIRCUIT_ELEMENT_TYPE_VALVE;}
    void rotate(bool clockwise) {dir_flip = dir_flip.rotate(clockwise);};
    void flip(bool vertically) {dir_flip = dir_flip.flip(vertically);};
    unsigned get_cost() {return 10;};
};

class CircuitElementSource : public CircuitElement
{

public:
    Direction direction = DIRECTION_N;

    CircuitElementSource(){}
    CircuitElementSource(Direction direction_):
        direction(direction_)
        {}
    CircuitElementSource(SaveObjectMap*);

    void save(SaveObjectMap*);
    virtual uint16_t get_desc();
    virtual CircuitElement* copy() { return new CircuitElementSource(direction);}
    unsigned getconnections(void);
    XYPos getimage(void);
    void sim_prep(PressureAdjacent adj, FastSim& fast_sim);
    CircuitElementType get_type() {return CIRCUIT_ELEMENT_TYPE_SOURCE;}
    void rotate(bool clockwise) {direction = direction_rotate(direction, clockwise);};
    void flip(bool vertically) {direction = direction_flip(direction, vertically);};
    unsigned get_cost() {return 5;};

};

class CircuitElementEmpty : public CircuitElement
{
public:
    CircuitElementEmpty(){}
    CircuitElementEmpty(SaveObjectMap*);

//    SaveObject* save();
    void save(SaveObjectMap*);
    virtual uint16_t get_desc();
    virtual CircuitElement* copy() { return new CircuitElementEmpty();}
    unsigned getconnections(void) {return 0;};
    XYPos getimage(void);
    void sim_prep(PressureAdjacent adj, FastSim& fast_sim) {};
    CircuitElementType get_type() {return CIRCUIT_ELEMENT_TYPE_EMPTY;}
    virtual bool is_empty() {return true;};
    void rotate(bool clockwise) {};
    void flip(bool vertically) {};
    unsigned get_cost() {return 0;};
};

class CircuitElementSubCircuit : public CircuitElement
{
private:
public:
    DirFlip dir_flip;
    int level_index;
    std::string name = "";
    Level* level = NULL;;
    Circuit* circuit = NULL;
    bool custom = false;
    bool read_only = false;

    CircuitElementSubCircuit(DirFlip dir_flip_, int level_index_, LevelSet* level_set, bool read_only_ = false);
    CircuitElementSubCircuit(SaveObjectMap*, bool read_only_ = false);
    CircuitElementSubCircuit(CircuitElementSubCircuit& other);
    ~CircuitElementSubCircuit();

    void save(SaveObjectMap*);
    virtual uint16_t get_desc();
    virtual CircuitElement* copy();
    void reset();
    void elaborate(LevelSet* level_set);
    void retire();
    bool contains_subcircuit_level(int level_index, LevelSet* level_set);
    unsigned getconnections(void);
    XYPos getimage(void);
    SDL_Rect getimage_bg(void);
    XYPos getimage_fg(void);
    WrappedTexture* getimage_fg_texture();
    void sim_prep(PressureAdjacent adj, FastSim& fast_sim);
    CircuitElementType get_type() {return CIRCUIT_ELEMENT_TYPE_SUBCIRCUIT;}
    Circuit* get_subcircuit(int *level_index_ = NULL) {if (level_index_) *level_index_ = level_index; return circuit;}
    virtual bool get_custom() {return custom;}
    virtual void set_custom(bool recurse = false);
    virtual bool get_read_only() {return read_only;}
    virtual void set_read_only(bool read_only_) {read_only = read_only_;}
    void rotate(bool clockwise) {dir_flip = dir_flip.rotate(clockwise);};
    void flip(bool vertically) {dir_flip = dir_flip.flip(vertically);};
    unsigned get_cost();
    virtual void reindex_deleted_level(LevelSet* level_set, int level_index);
};

class Sign
{
public:
    XYPos pos;
    XYPos size;
    Direction direction;
    std::string text;
    Sign(){};
    Sign(XYPos pos_, Direction direction_, std::string text);
    Sign(SaveObject* omap);
    SaveObject*  save();
    XYPos get_size();
    void set_size(XYPos size);
    XYPos get_pos();
    void rotate(bool clockwise);
    void flip(bool vertically);
};

class Circuit
{
public:
    class FastFunc
    {
    public:
        CircuitElement* element;
        PressureAdjacent adj;
        FastFunc(CircuitElement* element_, PressureAdjacent adj_):
            element(element_),
            adj(adj_)
        {}
    };

    CircuitElement* elements[9][9];

    std::list<Sign> signs;

    CircuitPressure connections_ns[10][10];
    CircuitPressure connections_ew[10][10];
    uint8_t touched_ns[10][10];
    uint8_t touched_ew[10][10];

    bool blocked[9][9] = {{false}};

    bool fast_prepped = false;
    std::vector<FastFunc> fast_funcs;

    FastSim fast_sim;
    
    std::list<Circuit*> undo_list;
    std::list<Circuit*> redo_list;


    Pressure last_vented = 0;
    Pressure last_moved = 0;

    Circuit(SaveObjectMap* omap);
    Circuit(Circuit& other);
    Circuit();
    ~Circuit();

    SaveObject* save(void);
    void copy_elements(Circuit& other);


    void remove_sign(std::list<Sign>::iterator it, bool no_history = false);
    void set_element_empty(XYPos pos, bool no_history = false);
    void set_element_pipe(XYPos pos, Connections con);
    void set_element_valve(XYPos pos, DirFlip dir_flip);
    void set_element_source(XYPos pos, Direction direction);
    void set_element_subcircuit(XYPos pos, DirFlip dir_flip, int level_index, LevelSet* level_set);
    void add_sign(Sign sign, bool no_undo = false);

    void move_selected_elements(std::set<XYPos> &selected_elements, Direction direction);
    void rotate_selected_elements(std::set<XYPos> &selected_elements, bool clockwise);
    void flip_selected_elements(std::set<XYPos> &selected_elements, bool vertically);
    void delete_selected_elements(std::set<XYPos> &selected_elements);
    void add_pipe_drag_list(std::list<XYPos> &pipe_drag_list);

    void reset();
    void elaborate(LevelSet* level_set);
    void retire();

    void render_prep();

    void sim_prep(PressureAdjacent adj, FastSim& fast_sim);
    void prep(PressureAdjacent);
    void sim_pre(PressureAdjacent);
    void remove_circles(LevelSet* level_set, std::set<unsigned> seen = {});
    void updated_ports() {fast_prepped = false;};
    void ammend();
    void force_element(XYPos pos, CircuitElement* element);
    void force_sign(Sign sign);
    bool is_blocked(XYPos pos);
    void undo(LevelSet* level_set);
    void redo(LevelSet* level_set);
    void paste(Clipboard& clipboard, XYPos pos, LevelSet* level_set);

    bool contains_subcircuit_level(int level_index, LevelSet* level_set);
    unsigned get_cost();
    void reset_steam_used() {fast_sim.reset_steam_used();}
    int64_t get_steam_used() {return fast_sim.get_steam_used();}
    SaveObjectList* save_forced();
    void copy_in(Circuit* other);
    void reindex_deleted_level(LevelSet* level_set, int level_index);
    void set_custom(bool recurse = false);

};

class Clipboard
{
public:
    class ClipboardElement
    {
    public:
        XYPos pos;
        CircuitElement* element;
        ClipboardElement(XYPos pos_, CircuitElement* element_):
            pos(pos_),
            element(element_)
        {}

    };
    std::list<ClipboardElement> elements;

    void copy(std::set<XYPos> &selected_elements, Circuit &circuit);
    void repos();
    XYPos size();
    void rotate(bool clockwise);
    void flip(bool vertically);
    void elaborate(LevelSet* level_set);

};
