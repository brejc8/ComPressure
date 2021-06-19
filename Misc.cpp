#include "Misc.h"

XYPos DirFlip::trans(XYPos pos, int size)
{
    size = size - 1;
    switch (dir)
    {
    case DIRECTION_N:
        break;
    case DIRECTION_E:
        pos = XYPos(size - pos.y, pos.x);
        break;
    case DIRECTION_S:
        pos = XYPos(size - pos.x,size - pos.y);
        break;
    case DIRECTION_W:
        pos = XYPos(pos.y, size - pos.x);
        break;
    }
    if (flp)
        pos.y = size - pos.y;
    return pos;
}
