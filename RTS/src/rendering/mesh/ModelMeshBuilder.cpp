#include "stdafx.h"
#include "ModelMeshBuilder.h"

#include "rendering/mesh/MeshBuilderCommon.h"
#include "rendering/texture/SubTexture.h"
#include "rendering/model/StaticModelInstance.h"

#include "rendering/model/Model3D.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/TriangleMesh.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

// TODO: Replace with texturerepo
#include "resources/TextureRepository.h"

#include "fbx/ozzFbxToMesh.hpp"
#include <fbxsdk/core/base/fbxstring.h>
#include <fbxsdk/scene/geometry/fbxlayer.h>

bool ModelMeshBuilder::buildStaticMeshesForModel(
    StaticModel3D& model,
    const vio::Path& filePath,
    const vio::Path& rootDir,
    OzzFbxSceneLoader& sceneLoader,
    MeshDrawMode drawMode,
    const TextureRepository& textureRepo,
    float modelScale
) {
    const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        pError("No mesh to process in this file: " + filePath.getString());
        return false;
    }

    // Read read material textures matched to material names
    std::vector<TextureHandle> textures;
    const int materialCount = sceneLoader.scene()->GetMaterialCount();
    textures.resize(materialCount * 2);
    for (int i = 0; i < materialCount; ++i) {
        FbxSurfaceMaterial* material = sceneLoader.scene()->GetMaterial(i);

        const nString materialName = material->GetName();
        const SubTexture& texture = textureRepo.getTexture(materialName);
        textures[i * 2] = texture.mTextureHandleDiffuse;
        textures[i * 2 + 1] = texture.mTextureHandleNormal;

        LOG_DEBUG("Material {} name {} ", i, materialName);
        for (FbxProperty matProp = material->GetFirstProperty(); matProp.IsValid(); matProp = material->GetNextProperty(matProp)) {
            LOG_DEBUG("  Property {}",  matProp.GetName().Buffer());
        }
    }

    model.mMesh = std::make_unique<Mesh>();

    // TODO: Per submesh textures

    mStaticVerts.clear();
    // Combine all submeshes into one mesh
    for (int m = 0; m < numMeshes; ++m) {

        FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(m);
        FbxLayerElementArrayTemplate<int>* pLockableArray;
        fbxMesh->GetMaterialIndices(&pLockableArray);
        LOG_DEBUG("    Material sttuff {} {}", pLockableArray->GetCount(), pLockableArray->GetFirst());

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
        const size_t prevSize = mStaticVerts.size();
        mStaticVerts.resize(mStaticVerts.size() + outputMesh.vertex_count());
        assert(outputMesh.parts.size() == 1);
        for (int i = 0; i < outputMesh.vertex_count(); ++i) {
            const ozzfbx::Mesh::Part& part = outputMesh.parts[0];
            StaticModelVertex& myVert = mStaticVerts[prevSize + i].mStaticModel;
            memcpy(&myVert.pos, &part.positions[(int)(i * 3)], sizeof(f32) * 3);
            myVert.pos *= modelScale;
            myVert.textureIndex = m; // TODO: Smarter
            f32v2 uvsFloat{ part.uvs[(int)i * 2], part.uvs[(int)i * 2 + 1] };
            assert(uvsFloat.x >= 0.0f && uvsFloat.x <= 1.0f && uvsFloat.y >= 0.0f && uvsFloat.y <= 1.0f);
            myVert.uvsPacked.x = (ui16)(uvsFloat.x * UINT16_MAX);
            myVert.uvsPacked.y = (ui16)(uvsFloat.y * UINT16_MAX);
           // memcpy(&myVert.normal, &part.normals[(int)(i * 3)], sizeof(f32) * 3);
           // memcpy(&myVert.tangent, &part.tangents[(int)(i * 3)], sizeof(f32) * 3);
           // memcpy(&myVert.uvs, &part.uvs[(int)(i * 2)], sizeof(f32) * 2);
            // TODO: Check materials for this mesh! See if each mesh has its own material data we can leverage
            if (part.colors.size()) {
                memcpy(&myVert.color, &part.colors[(int)(i * 4)], sizeof(uint8_t) * 4);
            }
            else {
                myVert.color = COLOR_WHITE;
            }
        }
        // Copy indices
        size_t start = mIndices.size();
        mIndices.resize(mIndices.size() + outputMesh.triangle_indices.size());
        
        for (int i = 0; i < outputMesh.triangle_index_count(); ++i) {
            mIndices[start + i] = outputMesh.triangle_indices[i] + prevSize; // Copy index data and shift index
        }
      

        timer.start();
    }

    // Upload mesh data
    SubMeshData* meshData = &model.mMesh->mMainMesh;

    PreciseTimer uploadTimer;
    MeshBuilderCommon::initMeshBuffers(*meshData, nullptr);
    MeshBuilderCommon::uploadIndexData(*meshData, mIndices.data(), mIndices.size(), drawMode);
    MeshBuilderCommon::uploadVertexData(*meshData, mStaticVerts, drawMode);
    MeshBuilderCommon::uploadStandardTextureUboData(*meshData, f32v3(0.0f), textures, drawMode);
    StaticModelVertex::bindVertexAttribs();
    checkGlError("ModelMeshBuilder::buildStaticMeshesForModel");

    LOG_TRACE("  Upload data in {} ms", uploadTimer.stop());

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

void ModelMeshBuilder::updateInstanceDataForStaticModel(const Mesh& mesh, VGBuffer instanceDataVbo) {
    const SubMeshData* meshData = &mesh.mMainMesh;
    do {
        glBindVertexArray(meshData->mVao);
        glBindBuffer(GL_ARRAY_BUFFER, instanceDataVbo);
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(StaticModelInstance), (void*)offsetof(StaticModelInstance, pos));
        glVertexAttribDivisor(7, 1);
        meshData = meshData->mNextSubmesh;
    } while (meshData != nullptr);
    glBindVertexArray(0);
}
