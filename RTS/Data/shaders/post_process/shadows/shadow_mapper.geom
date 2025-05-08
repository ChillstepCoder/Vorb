layout(triangles, invocations = 4) in; // TODO: Make invocation count a define
layout(triangle_strip, max_vertices = 3) out;
    
uniform mat4 unShadowFrustumMatrices[4]; // match invocation count
    
void main()
{          
    for (int i = 0; i < 4; ++i)
    {
        gl_Position = unShadowFrustumMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}  