#include "stdafx.h"
#include "ModelMeshBuilder.h"

#include "rendering/mesh/MeshBuilderCommon.h"
#include "rendering/texture/SubTexture.h"

#include "rendering/model/Model3D.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/TriangleMesh.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

// TODO: Replace with texturerepo
#include "resources/TextureRepository.h"

#include "fbx/ozzFbxToMesh.hpp"

bool ModelMeshBuilder::buildStaticMeshesForModel(
    StaticModel3D& model,
    const vio::Path& filePath,
    const vio::Path& rootDir,
    OzzFbxSceneLoader& sceneLoader,
    MeshDrawMode drawMode,
    const TextureRepository& textureRepo
) {
    const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        pError("No mesh to process in this file: " + filePath.getString());
        return false;
    }

    model.mMesh = std::make_unique<Mesh>();
    model.mNumMeshes = numMeshes;

    // TODO: All textures
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    const SubTexture& texture = textureRepo.getTexture(modelFileNameNoExtension);

    SubMeshData* meshData = &model.mMesh->mMainMesh;
    meshData->allocateSubmeshCount(numMeshes - 1, false);

    for (int m = 0; m < numMeshes; ++m) {
        assert(meshData);

        FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(m);

        PreciseTimer timer;
        // Allocates output mesh.
        ozzfbx::Mesh outputMesh;
        outputMesh.parts.resize(1);

        ControlPointsRemap remap;
        // TODO: Non OZZ version so we dont have an intermediate conversion
        if (!BuildVertices(fbxMesh, sceneLoader.converter(), &remap, &outputMesh)) {
            pError("Failed to read vertices: " + filePath.getString());
            return false;
         }

        // TODO: https://www.khronos.org/opengl/wiki/Normalized_Integer#Alternate_mapping
        // https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
        mStaticVerts.resize(outputMesh.vertex_count());
        assert(outputMesh.parts.size() == 1);
        for (int i = 0; i < outputMesh.vertex_count(); ++i) {
            const ozzfbx::Mesh::Part& part = outputMesh.parts[0];
            StaticModelVertex& myVert = mStaticVerts[i];
            memcpy(&myVert.pos, &part.positions[(int)(i * 3)], sizeof(f32) * 3);
           // memcpy(&myVert.normal, &part.normals[(int)(i * 3)], sizeof(f32) * 3);
           // memcpy(&myVert.tangent, &part.tangents[(int)(i * 3)], sizeof(f32) * 3);
           // memcpy(&myVert.uvs, &part.uvs[(int)(i * 2)], sizeof(f32) * 2);
            if (part.colors.size()) {
                memcpy(&myVert.color, &part.colors[(int)(i * 4)], sizeof(uint8_t) * 4);
            }
            else {
                myVert.color = COLOR_WHITE;
            }
        }
        
        initStaticMeshBuffers(*meshData);
        uploadStaticMeshData(*meshData, outputMesh.triangle_indices.data(), outputMesh.triangle_index_count(), MeshDrawMode::STATIC);
        mStaticVerts.clear();
        LOG_TRACE("  Copy data in {} ms", timer.stop());
        timer.start();

        // Next submesh
        meshData = meshData->mNextSubmesh;
    }

    glBindVertexArray(0);
    return true;
}

bool ModelMeshBuilder::buildSkinnedMeshesForModel(
    SkinnedModel3D& model,
    const ozz::animation::Skeleton& skeleton,
    const vio::Path& filePath,
    const vio::Path& rootDir,
    OzzFbxSceneLoader& sceneLoader,
    MeshDrawMode drawMode,
    const TextureRepository& textureRepo
) {

    const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        pError("No mesh to process in this file: " + filePath.getString());
        return false;
    }

    model.mSkinnedMeshes = std::unique_ptr<SkinnedMesh[]>(new SkinnedMesh[numMeshes]);
    model.mNumMeshes = numMeshes;

    for (int m = 0; m < numMeshes; ++m) {

        FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(m);

        SkinnedMesh& outMesh = model.mSkinnedMeshes[m];

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
    }


    // Find and load textures
    // Right now, textures are shared with every mesh in the scene
    // TODO: Allow different textures per submesh?
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();

    const SubTexture& texture = textureRepo.getTexture(modelFileNameNoExtension);
    for (int i = 0; i < numMeshes; ++i) {
        model.mSkinnedMeshes[i].setDiffuseTexture(texture.mTextureDiffuse);
    }
    for (int i = 0; i < numMeshes; ++i) {
        model.mSkinnedMeshes[i].setNormalTexture(texture.mTextureNormal);
    }

    // TODO: Store this
    VGTexture tex = textureRepo.getTexture(modelFileNameNoExtension + ".spec").mTextureDiffuse;
    for (int i = 0; i < numMeshes; ++i) {
        model.mSkinnedMeshes[i].setSpecularTexture(tex);
    }

    ui8 numSkinningMatrices = 0;
    for (ui32 i = 0; i < model.getNumMeshes(); ++i) {
        numSkinningMatrices = std::max(numSkinningMatrices, model.getMeshes()[i].getNumJoints());
    }
    model.mNumSkinningMatrices = numSkinningMatrices;

    glBindVertexArray(0);
    return true;
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

void ModelMeshBuilder::uploadStaticMeshData(SubMeshData& subMesh, const uint16_t* indices, int indexCount, MeshDrawMode drawMode) {
    //glBindVertexArray(subMesh.mVao);

    //// IBO
    //// Non shared IBO
    //// TODO: Support ui16 compression
    //subMesh.mIndexCount = indexCount;
    //subMesh.mIndexType = GL_UNSIGNED_SHORT;
    //const ui32 indexBufferSizeBytes = indexCount * sizeof(uint16_t);
    //// Allocate orphaned
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subMesh.mIbo);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBufferSizeBytes, nullptr, e_cast(drawMode));
    //// Set data
    //glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexBufferSizeBytes, indices);

    //// VBO
    //const size_t vertexCount = mStaticVerts.size();
    //const unsigned bufferSizeBytes = vertexCount * sizeof(StaticModelVertex);
    //// Allocate orphaned
    //glBindBuffer(GL_ARRAY_BUFFER, subMesh.mVbo);
    //glBufferData(GL_ARRAY_BUFFER, bufferSizeBytes, nullptr, e_cast(drawMode));
    //// Set data
    //glBufferSubData(GL_ARRAY_BUFFER, 0, bufferSizeBytes, mStaticVerts.data());

    //// UBO
    //assert(subMesh.mUbo);
    //const ui32 uboSizeBytes = sizeof(f32v4) + mTextures.size() * sizeof(TextureHandle);
    //// Pack into uvec2 - https://www.khronos.org/opengl/wiki/Bindless_Texture
    //// With position in front
    //constexpr size_t BUFFER_SIZE = sizeof(f32v4) + MAX_TEXTURES_PER_MESH * 2 * sizeof(ui32v2);
    //ui8 byteBuffer[BUFFER_SIZE];
    //*(f32v3*)byteBuffer = f32v3(0.0f); // Zero position for now?
    //ui32v2* buffer = (ui32v2*)(byteBuffer + sizeof(f32v4));
    //for (ui32 i = 0; i < mTextures.size(); ++i) {
    //    TextureHandle handle = mTextures[i];
    //    buffer[i].x = handle & 0xffffffff;
    //    buffer[i].y = handle >> 32;
    //}
    //// Allocate orphaned
    //glBindBuffer(GL_UNIFORM_BUFFER, subMesh.mUbo);
    //glBufferData(GL_UNIFORM_BUFFER, uboSizeBytes, nullptr, e_cast(drawMode));
    //// Set data
    //glBufferSubData(GL_UNIFORM_BUFFER, 0, uboSizeBytes, byteBuffer);

    //checkGlError("MeshBuilder::uploadMeshData");

    //bindStaticVertexAttribs(subMesh);
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
