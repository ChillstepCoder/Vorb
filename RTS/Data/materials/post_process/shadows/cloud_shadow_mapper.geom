layout(triangles, invocations = 4) in; // TODO: Make invocation count a define
layout(triangle_strip, max_vertices = 3) out;
    
uniform mat4 ShadowFrustumMatrices[4]; // match invocation count

in vec2 gUV[3];
flat in float gAtlasPage[3];

out vec2 fUV;
flat out float fAtlasPage;
    
void main()
{          
    for (int i = 0; i < gl_in.length(); ++i)
    {
        gl_Position = ShadowFrustumMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
		fUV = gUV[i];
		fAtlasPage = gAtlasPage[i];
        EmitVertex();
    }
    EndPrimitive();
}  