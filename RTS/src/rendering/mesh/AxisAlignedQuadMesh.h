#pragma once
// Std430 layout
struct AxisAlignedQuadData {
    f32v2 pos;
    f32v2 dims;
    color4 color;
    ui8 padding[4];
};

class AxisAlignedQuadMesh
{
public:
    AxisAlignedQuadMesh();
    ~AxisAlignedQuadMesh();

    void initialize(const std::vector<AxisAlignedQuadData>& quads);

    bool isValid() const { return mVao != 0; }

    // Call before drawing
    void bind();

    void drawQuads();

    VGBuffer mVao = 0;
    VGBuffer mSsbo = 0;
    ui32 mNumQuads = 0;
};
