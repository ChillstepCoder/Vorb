#pragma once

struct LineVertex {
    f32v3 pos;
    color4 color;
};

class LineMesh
{
public:
    LineMesh();
    ~LineMesh();

    void initialize(const std::vector<LineVertex>& vertices);

    bool isValid() const { return mVao != 0; }

    // Call before drawing
    void bind();

    void drawLineStrip(int start, ui32 count) const;
    void drawPoints(int start, ui32 count) const;
    void drawLines(int start, ui32 count) const;

    VGBuffer mVao = 0;
    VGBuffer mVbo = 0;
};

