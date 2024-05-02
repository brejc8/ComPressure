#pragma once
#define _USE_MATH_DEFINES
#include <cmath>

#include <cassert>
#include <cstdint>
#include <iostream>

#define BREAKPOINT __asm__ volatile("int $0x03");

enum Direction
{
    DIRECTION_N = 0,
    DIRECTION_E = 1,
    DIRECTION_S = 2,
    DIRECTION_W = 3
};

inline Direction direction_rotate(Direction direction, bool clockwise)
{
    if (clockwise)
        return Direction((int(direction) + 1) % 4);
    else
        return Direction((int(direction) + 4 - 1) % 4);
}

inline Direction direction_flip(Direction direction, bool vertically)
{
    if ((int(direction) ^ vertically) & 1)
        return Direction((int(direction) + 2) % 4);
    else
        return direction;
}

class XYPos;
class DirFlip
{
public:
    Direction dir = DIRECTION_N;
    bool flp = false;
    DirFlip() {}
    DirFlip(Direction dir_, bool flp_):
        dir(dir_),
        flp(flp_)
    {}

    DirFlip(int in):
        dir(Direction(in & 0x3)),
        flp((in >> 2)& 0x1)
    {}
    
    Direction get_n()
    {
        return get_dir(DIRECTION_N);
    }

    Direction get_dir(Direction t)
    {
        t = Direction((int(dir) + int(t)) % 4);
        if (!flp)
            return t;
        if (t == DIRECTION_N)
            return DIRECTION_S;
        if (t == DIRECTION_S)
            return DIRECTION_N;
        return t;
    }
    
    unsigned mask(unsigned in)
    {
        in <<= int(dir);
        in |= in >> 4;
        if (flp)
        {
            in = (in & 0xA) | ((in & 1) << 2) | ((in >> 2) & 1);
        }
        return in & 0xF;
    }

    int as_int()
    {
        return int(dir) + (int(flp) << 2);
    }

    DirFlip rotate(bool clockwise)
    {
        if (clockwise != flp)
            return DirFlip(Direction((int(dir) + 1) % 4), flp);
        else
            return DirFlip(Direction((int(dir) + 3) % 4), flp);
    }

    DirFlip flip(bool vertically)
    {
        if (vertically)
            return DirFlip(dir, !flp);
        return DirFlip(Direction((int(dir) + 2) % 4), !flp);
    }
    XYPos trans(XYPos pos, int size);
    XYPos trans_inv(XYPos pos, int size);
};

class Rand
{
public:
    unsigned value;
    Rand(unsigned i):
        value(i)
    {};

    operator unsigned int()
    {
        value = (value >> 1) ^ (value & 1 ? 0: 0x90000000);
        return value & 0x7FFFFFFF;
    };
    unsigned int save()
    {
        return value;
    };
};


class XYPosFloat;

class XYPos
{
public:
    int x;
    int y;
    
    XYPos():
        x(0),
        y(0)
    {}
    XYPos(XYPosFloat pos);
    XYPos(int x_, int y_):
        x(x_),
        y(y_)
    {}

    XYPos(Direction dir)
    {
        static const XYPos dirpos[4] = {{0,-1}, {1,0}, {0,1}, {-1,0}};
        *this = dirpos[dir%4];
    }

    bool operator<(const XYPos& other) const
    {
        return x < other.x || (!(other.x < x) && y < other.y);
    }

    operator bool() const
    {
        return x || y;
    }

    bool operator==(const XYPos& other) const
    {
        return (x == other.x) && (y == other.y);
    }

    bool operator!=(const XYPos& other) const
    {
        return (x != other.x) || (y != other.y);
    }

    XYPos operator-(const XYPos& other) const
    {
        return XYPos(x - other.x, y - other.y);
    }

    XYPos operator+(const XYPos& other) const
    {
        return XYPos(x + other.x, y + other.y);
    }

    XYPos operator*(const XYPos& other) const
    {
        return XYPos(x * other.x, y * other.y);
    }

    XYPos operator*(const int& other) const
    {
        return XYPos(x * other, y * other);
    }

    XYPos operator*(const Direction& other) const
    {
        switch (other)
        {
        case DIRECTION_N:
            return XYPos(x, y);
        case DIRECTION_E:
            return XYPos(-y, x);
        case DIRECTION_S:
            return XYPos(-x,-y);
        case DIRECTION_W:
            return XYPos(y, -x);
        }
        assert(0);
    }

    XYPos operator/(const int& other) const
    {
        return XYPos(x / other, y / other);
    }

    void operator+=(const XYPos& other)
    {
        x += other.x;
        y += other.y;
    }

    void operator*=(int mul)
    {
        x *= mul;
        y *= mul;
    }

    void operator-=(const XYPos& other)
    {
        x -= other.x;
        y -= other.y;
    }

    void operator/=(const int d)
    {
        x /= d;
        y /= d;
    }

    bool inside(const XYPos& other)
    {
        return (x >= 0 && y >= 0 && x < other.x && y < other.y);
    }

    void clamp(const XYPos& min, const XYPos& max)
    {
        if (x < min.x) x = min.x;
        if (y < min.y) y = min.y;
        if (x > max.x) x = max.x;
        if (y > max.y) y = max.y;
    }

    void iter_next(const XYPos& other)
    {
        x++;
        if (x >= other.x)
        {
            x = 0;
            y++;
        }
    }

    bool iter_cond(const XYPos& other)
    {
        return y < other.y;
    }
};

#define CENTURN_IN_RAD ((M_PI * 2) / 100)

class Angle
{
public:
    double angle;
    Angle(double angle_):
        angle(angle_)
    {};

    operator double() const
    {
        return angle;
    }

    Angle operator^(const Angle& other) const
    {
        double dif = fmod(angle - other.angle + M_PI, M_PI * 2);
        if (dif < 0)
            dif += M_PI * 2;
        return Angle(dif - M_PI);
    }
    
    Angle abs() const
    {
        return Angle(fabs(angle));
    }

};

class XYPosFloat
{
public:
    double x;
    double y;

    XYPosFloat():
        x(0),
        y(0)
    {}
    XYPosFloat(XYPos pos):
        x(pos.x),
        y(pos.y)
    {}
    XYPosFloat(XYPos pos, double mul):
        x(pos.x * mul),
        y(pos.y * mul)
    {}
    XYPosFloat(double x_, double y_):
        x(x_),
        y(y_)
    {}

    XYPosFloat(Angle a, double d):
        x(sin(a.angle) * d),
        y(cos(a.angle) * d)
    {}

    bool operator<(const XYPosFloat& other) const
    {
        return x < other.x || (!(other.x < x) && y < other.y);
    }

    operator bool() const
    {
        return (x != 0) || (y != 0);
    }

    bool operator==(const XYPosFloat& other) const
    {
        return (x == other.x) && (y == other.y);
    }

    bool operator!=(const XYPos& other) const
    {
        return (x != other.x) || (y != other.y);
    }

    XYPosFloat operator-(const XYPosFloat& other) const
    {
        return XYPosFloat(x - other.x, y - other.y);
    }

    XYPosFloat operator+(const XYPosFloat& other) const
    {
        return XYPosFloat(x + other.x, y + other.y);
    }

    XYPosFloat operator*(const XYPosFloat& other) const
    {
        return XYPosFloat(x * other.x, y * other.y);
    }

    XYPosFloat operator*(const double& other) const
    {
        return XYPosFloat(x * other, y * other);
    }

    XYPosFloat operator*(const int& other) const
    {
        return XYPosFloat(x * other, y * other);
    }

    XYPosFloat operator/(const double& other) const
    {
        return XYPosFloat(x / other, y / other);
    }

    void operator+=(const XYPosFloat& other)
    {
        x += other.x;
        y += other.y;
    }

    void operator*=(double mul)
    {
        x *= mul;
        y *= mul;
    }

    void operator-=(const XYPosFloat& other)
    {
        x -= other.x;
        y -= other.y;
    }

    void operator/=(const double d)
    {
        x /= d;
        y /= d;
    }

    double distance(const XYPosFloat& other) const
    {
        return sqrt((x - other.x)*(x - other.x) + 
                    (y - other.y)*(y - other.y));
    }

    double distance() const
    {
        return sqrt(x * x + y * y);
    }

    double angle(const XYPosFloat& other) const
    {
        return atan2(other.x - x, other.y - y);
    }

    double angle() const
    {
        return atan2(x, y);
    }

    XYPosFloat rotate(Angle delta) const
    {
        double r = distance();
        Angle feta = angle() + delta;
        return XYPosFloat(feta, r);
    }

};


inline XYPos::XYPos(XYPosFloat pos):
        x(floor(pos.x)),
        y(floor(pos.y))
{}

inline std::ostream& operator<<(std::ostream& os, const XYPosFloat& obj)
{
      os << "(" << obj.x << ", " << obj.y << ")";
      return os;
}

inline unsigned popcount(unsigned in)
{
    unsigned count = 0;
    while (in)
    {
        count += in & 1;
        in >>= 1;
    }
    return count;   
}

inline bool is_leading_utf8_byte(char c)
{
    return (c & 0xC0) != 0x80;
}

uint64_t checksum(std::string s);
