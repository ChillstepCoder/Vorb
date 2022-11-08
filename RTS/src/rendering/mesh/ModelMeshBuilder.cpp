#include "stdafx.h"
#include "ModelMeshBuilder.h"

#include "rendering/texture/SubTexture.h"

#include "rendering/model/Model3D.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/TriangleMesh.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include "fbx/ozzFbxToMesh.hpp"

bool ModelMeshBuilder::buildStaticMesh(
    Mesh& outMesh,
    int meshIndex,
    const vio::Path& filePath,
    OzzFbxSceneLoader& sceneLoader,
    MeshDrawMode drawMode
) {
    FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(meshIndex);

    PreciseTimer timer;
    // Allocates output mesh.
    ozzfbx::Mesh outputMesh;
    outputMesh.parts.resize(1);

    ControlPointsRemap remap;
    if (!BuildVertices(fbxMesh, sceneLoader.converter(), &remap, &outputMesh)) {
        pError("Failed to read vertices: " + filePath.getString());
        return false;
    }

    // Finds skinning informations
    //if (fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0) {
    //    if (!BuildSkin(fbxMesh, sceneLoader.converter(), remap, skeleton, &outputMesh)) {
    //        pError("Failed to read skinning data: " + filePath.getString());
    //        return false;
    //    }
    //    LOG_TRACE("  Build skin in {} ms", timer.stop());
    //    timer.start();
    //    // Limiting number of joint influences per vertex.
    //    if (!LimitInfluences(outputMesh, MAX_BONES_PER_VERTEX)) {
    //        pError("Failed to limit number of joint influences: " + filePath.getString());
    //        return false;
    //    }
    //    LOG_TRACE("  Limit influences in {} ms", timer.stop());
    //    timer.start();
    //    // Remap joint indices. The mesh might not use all skeleton joints, so
    //    // this function remaps joint indices to the subset of used joints. It
    //    // also reoders inverse bin pose matrices.
    //    if (!RemapIndices(&outputMesh)) {
    //        pError("Failed to remap joint indices: " + filePath.getString());
    //        return false;
    //    }

    //    LOG_TRACE("  Remap indices in {} ms", timer.stop());
    //    timer.start();
    //    // Split the mesh if option is true (default)
    //    //if (OPTIONS_split) {
    //    //    ozz::sample::Mesh partitioned_meshes;
    //    //    if (!SplitParts(output_mesh, &partitioned_meshes)) {
    //    //        ozz::log::Err() << "Failed to partitioned meshes." << std::endl;
    //    //        return EXIT_FAILURE;
    //    //    }

    //    //    // Copy partitioned mesh back to the output.
    //    //    output_mesh = partitioned_meshes;
    //    //}

    //    if (!StripWeights(&outputMesh)) {
    //        pError("Failed to strip weights: " + filePath.getString());
    //        return false;
    //    }
    //    LOG_TRACE("  Strip weights in {} ms", timer.stop());
    //    timer.start();

    //    assert(outputMesh.max_influences_count() <= MAX_BONES_PER_VERTEX);
    //}
    mStaticVerts.resize(outputMesh.vertex_count());
    assert(outputMesh.parts.size() == 1);
    LOG_CRITICAL(" NEED TO FIX normals ModelMeshBuilder::buildStaticMesh");
    for (int i = 0; i < outputMesh.vertex_count(); ++i) {
        const ozzfbx::Mesh::Part& part = outputMesh.parts[0];
        StaticModelVertex& myVert = mStaticVerts[i];
        memcpy(&myVert.pos, &part.positions[(int)(i * 3)], sizeof(f32) * 3);
            
        // TODO: https://www.khronos.org/opengl/wiki/Normalized_Integer#Alternate_mapping
        // https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
        myVert.normalPacked = {};
        myVert.tangentPacked = {};
        //memcpy(&myVert.normal, &part.normals[(int)(i * 3)], sizeof(f32) * 3);
        //memcpy(&myVert.tangent, &part.tangents[(int)(i * 3)], sizeof(f32) * 3);
        f32v2 uvs;
        memcpy(&uvs, &part.uvs[(int)(i * 2)], sizeof(f32) * 2);
        assert(uvs.x >= 0.0f && uvs.y >= 0.0f && uvs.x <= 1.0f && uvs.y <= 1.0f);
        myVert.uvsPacked = ui16v2(uvs.x * UINT16_MAX, uvs.y * UINT16_MAX);
        if (part.colors.size()) {
            memcpy(&myVert.color, &part.colors[(int)(i * 4)], sizeof(uint8_t) * 4);
        }
        else {
            myVert.color = COLOR_WHITE;
        }
    }

    // Allocate all buffers if needed
    SubMeshData* subMesh = &outMesh.mMainMesh;
    do {
        initStaticMeshBuffers(*subMesh);
        subMesh = subMesh->mNextSubmesh;
    } while (subMesh != nullptr);

    uploadStaticMeshData(outMesh.mMainMesh, outputMesh.triangle_indices.data(), outputMesh.triangle_index_count(), drawMode);

    //mSkinnedVerts.clear();
    LOG_TRACE("  Copy data in {} ms", timer.stop());
    timer.start();

    glBindVertexArray(0);
}

bool ModelMeshBuilder::buildSkinnedMesh(
    SkinnedMesh& outMesh,
    int meshIndex,
    const ozz::animation::Skeleton& skeleton,
    const vio::Path& filePath,
    OzzFbxSceneLoader& sceneLoader,
    MeshDrawMode drawMode
) {

    FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(meshIndex);

    PreciseTimer timer;
    // Allocates output mesh.
    ozzfbx::Mesh outputMesh;
    outputMesh.parts.resize(1);

    ControlPointsRemap remap;
    if (!BuildVertices(fbxMesh, sceneLoader.converter(), &remap, &outputMesh)) {
        pError("Failed to read vertices: " + filePath.getString());
        return false;
    }

    // Finds skinning informations
    if (fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0) {
        if (!BuildSkin(fbxMesh, sceneLoader.converter(), remap, skeleton, &outputMesh)) {
            pError("Failed to read skinning data: " + filePath.getString());
            return false;
        }
        LOG_TRACE("  Build skin in {} ms", timer.stop());
        timer.start();
        // Limiting number of joint influences per vertex.
        if (!LimitInfluences(outputMesh, MAX_BONES_PER_VERTEX)) {
            pError("Failed to limit number of joint influences: " + filePath.getString());
            return false;
        }
        LOG_TRACE("  Limit influences in {} ms", timer.stop());
        timer.start();
        // Remap joint indices. The mesh might not use all skeleton joints, so
        // this function remaps joint indices to the subset of used joints. It
        // also reoders inverse bin pose matrices.
        if (!RemapIndices(&outputMesh)) {
            pError("Failed to remap joint indices: " + filePath.getString());
            return false;
        }

        LOG_TRACE("  Remap indices in {} ms", timer.stop());
        timer.start();
        // Split the mesh if option is true (default)
        //if (OPTIONS_split) {
        //    ozz::sample::Mesh partitioned_meshes;
        //    if (!SplitParts(output_mesh, &partitioned_meshes)) {
        //        ozz::log::Err() << "Failed to partitioned meshes." << std::endl;
        //        return EXIT_FAILURE;
        //    }

        //    // Copy partitioned mesh back to the output.
        //    output_mesh = partitioned_meshes;
        //}

        if (!StripWeights(&outputMesh)) {
            pError("Failed to strip weights: " + filePath.getString());
            return false;
        }
        LOG_TRACE("  Strip weights in {} ms", timer.stop());
        timer.start();

        assert(outputMesh.max_influences_count() <= MAX_BONES_PER_VERTEX);

        mSkinnedVerts.resize(outputMesh.vertex_count());
        assert(outputMesh.parts.size() == 1);
        for (int i = 0; i < outputMesh.vertex_count(); ++i) {
            const ozzfbx::Mesh::Part& part = outputMesh.parts[0];
            SkinnedModelVertex& myVert = mSkinnedVerts[i];
            memcpy(&myVert.pos, &part.positions[(int)(i * 3)], sizeof(f32) * 3);
            memcpy(&myVert.normal, &part.normals[(int)(i * 3)], sizeof(f32) * 3);
            memcpy(&myVert.tangent, &part.tangents[(int)(i * 3)], sizeof(f32) * 3);
            memcpy(&myVert.uvs, &part.uvs[(int)(i * 2)], sizeof(f32) * 2);
            if (part.colors.size()) {
                memcpy(&myVert.color, &part.colors[(int)(i * 4)], sizeof(uint8_t) * 4);
            }
            else {
                myVert.color = COLOR_WHITE;
            }
            // TODO: Shrink to ui8?
            int influencesCount = part.influences_count();
            int j = 0;
            for (int j = 0; j < influencesCount; ++j) {
                myVert.boneIDs[j] = (ui8)part.joint_indices[(int)(i * influencesCount + j)];
            }

            // Zero the weight first since we are re-using vertex buffers
            memset(myVert.boneWeights, 0, sizeof(f32) * MAX_BONES_PER_VERTEX);
            if (influencesCount == 1) {
                myVert.boneWeights[0] = 1.0f;
            }
            else {
                memcpy(myVert.boneWeights, &part.joint_weights[(int)(i * (influencesCount - 1))], sizeof(f32) * f32(influencesCount - 1));
            }
        }
        const int numJoints = outputMesh.num_joints();
        assert(numJoints < 100);
        outMesh.mJointRemaps = std::unique_ptr<ui8[]>(new ui8[numJoints]);
        outMesh.mInverseBindPoses = std::unique_ptr<ozz::math::Float4x4[]>(new ozz::math::Float4x4[numJoints]);
        for (int i = 0; i < numJoints; ++i) {
            outMesh.mJointRemaps[i] = (ui8)outputMesh.joint_remaps[i];
            outMesh.mInverseBindPoses[i] = outputMesh.inverse_bind_poses[i];
        }
        outMesh.mNumJoints = numJoints;
        outMesh.setData(mSkinnedVerts.data(), (ui32)mSkinnedVerts.size(), MeshDrawMode::STATIC);
        outMesh.setIndices(outputMesh.triangle_indices.data(), outputMesh.triangle_index_count());
        mSkinnedVerts.clear();
        LOG_TRACE("  Copy data in {} ms", timer.stop());
        timer.start();
    }

    glBindVertexArray(0);
}

void ModelMeshBuilder::initStaticMeshBuffers(SubMeshData& subMesh) {
    // VAO
    if (subMesh.mVao == 0) {
        glGenVertexArrays(1, &subMesh.mVao);
        glBindVertexArray(subMesh.mVao);
        glGenBuffers(1, &subMesh.mVbo);
        glGenBuffers(1, &subMesh.mUbo);
        glGenBuffers(1, &subMesh.mIbo);
    }
    else {
        glBindVertexArray(subMesh.mVao);
    }

    glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);

    glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1 /*index*/, subMesh.mUbo);

    checkGlError("MeshBuilder::initStaticMeshBuffers");
}

void ModelMeshBuilder::uploadStaticMeshData(SubMeshData& subMesh, const uint16_t* indices, int indexCount, MeshDrawMode drawMode)
{
    glBindVertexArray(subMesh.mVao);

    // IBO
    // Non shared IBO
    // TODO: Support ui16 compression
    subMesh.mIndexCount = indexCount;
    subMesh.mIndexType = GL_UNSIGNED_SHORT;
    const ui32 indexBufferSizeBytes = indexCount * sizeof(uint16_t);
    // Allocate orphaned
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBufferSizeBytes, indices);

    // VBO
    const size_t vertexCount = mStaticVerts.size();
    const unsigned bufferSizeBytes = vertexCount * sizeof(StaticModelVertex);
    // Allocate orphaned
    glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, mStaticVerts.data());

    // UBO
    assert(subMesh.mUbo);
    const ui32 uboSizeBytes = sizeof(f32v4) + mTextures.size() * sizeof(TextureHandle);
    // Pack into uvec2 - https://www.khronos.org/opengl/wiki/Bindless_Texture
    // With position in front
    constexpr size_t BUFFER_SIZE = sizeof(f32v4) + MAX_TEXTURES_PER_MESH * 2 * sizeof(ui32v2);
    ui8 byteBuffer[BUFFER_SIZE];
    *(f32v3*)byteBuffer = f32v3(0.0f); // Zero position for now?
    ui32v2* buffer = (ui32v2*)(byteBuffer + sizeof(f32v4));
    for (ui32 i = 0; i < mTextures.size(); ++i) {
        TextureHandle handle = mTextures[i];
        buffer[i].x = handle & 0xffffffff;
        buffer[i].y = handle >> 32;
    }
    // Allocate orphaned
    glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
    glBufferData(GL_UNIFORM_BUFFER, uboSizeBytes, nullptr, e_cast(drawMode));
    // Set data
    glBufferSubData(GL_UNIFORM_BUFFER, 0, uboSizeBytes, byteBuffer);

    checkGlError("MeshBuilder::uploadMeshData");

    bindStaticVertexAttribs(subMesh);
}

void ModelMeshBuilder::bindStaticVertexAttribs(SubMeshData& subMesh)
{
    // Standard verts
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0 /*index*/, 3 /*size*/, GL_FLOAT, false, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1 /*index*/, 2 /*size*/, GL_UNSIGNED_SHORT, false, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, uvsPacked));
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2 /*index*/, 1 /*size*/, GL_UNSIGNED_BYTE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, textureIndex));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3 /*index*/, 4 /*size*/, GL_UNSIGNED_BYTE, true, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, color));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4 /*index*/, 3 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, normalPacked));
    glEnableVertexAttribArray(5);
    assert(false && "Check that size in the shader is 3, in standard_tile it is 2");
    //glVertexAttribPointer(4 /*index*/, 3 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, normalPacked));
    glVertexAttribPointer(5 /*index*/, 3 /*size*/, GL_INT_2_10_10_10_REV, GL_TRUE, sizeof(StaticModelVertex), (void*)offsetof(StaticModelVertex, tangentPacked));
}
