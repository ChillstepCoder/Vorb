#pragma once

struct GridEdge {
    TileIndex start;
    TileIndex end;
    Cartesian edgeDir; // Clockwise winding
    ui16 length = 1u;
};