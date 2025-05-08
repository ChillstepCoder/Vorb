#pragma once
// Std430 layout. TODO: Store color seperately? We dont want HDR
struct AxisAlignedQuadData {
    f32v2 pos;
    f32v2 dims;
    f32v4 color;
};

class AxisAlignedQuadMesh
{
public:
    AxisAlignedQuadMesh();
    ~AxisAlignedQuadMesh();

    void initialize(const std::vector<AxisAlignedQuadData>& quads);

    bool isValid() const { return mVao != 0; }

    // Call before drawing
    void bind(GLuint ssboIndex);

    void drawQuads();

    VGBuffer mVao = 0;
    VGBuffer mSsbo = 0;
    ui32 mNumQuads = 0;
};
