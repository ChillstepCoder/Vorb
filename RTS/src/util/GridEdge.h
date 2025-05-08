#pragma once

struct GridEdge {
    TileIndex start;
    TileIndex end;
    ui16 length = 1u;
    Cartesian edgeDir; // Clockwise winding
    //ui8 padding;
};