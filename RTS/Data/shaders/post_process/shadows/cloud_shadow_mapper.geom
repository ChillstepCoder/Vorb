layout(triangles, invocations = 4) in; // TODO: Make invocation count a define
layout(triangle_strip, max_vertices = 3) out;
    
uniform mat4 unShadowFrustumMatrices[4]; // match invocation count

in vec2 gUV[3];
flat in uint gMaterialIndex[3];

out vec2 fUV;
flat out uint fMaterialIndex;
    
void main()
{          
    for (int i = 0; i < gl_in.length(); ++i)
    {
        gl_Position = unShadowFrustumMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
		fUV = gUV[i];
		fMaterialIndex = gMaterialIndex[i];
        EmitVertex();
    }
    EndPrimitive();
}  