#include "stdafx.h"
#include "ModelMeshBuilder.h"

#include "rendering/mesh/MeshBuilderCommon.h"
#include "rendering/texture/SubTexture.h"
#include "rendering/model/StaticModelInstance.h"

#include "rendering/model/Model3D.h"
#include "rendering/mesh/RawMesh.h"
#include "rendering/mesh/Mesh.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include "resources/MaterialRepository.h"

#include "fbx/ozzFbxToMesh.hpp"
#include <fbxsdk/core/base/fbxstring.h>
#include <fbxsdk/scene/geometry/fbxlayer.h>

bool ModelMeshBuilder::buildSkinnedMeshesForModel(
    SkinnedModel3D& model,
    const ozz::animation::Skeleton& skeleton,
    const vio::Path& filePath,
    OzzFbxSceneLoader& sceneLoader,
    MeshDrawMode drawMode,
    const MaterialRepository& materialRepo
) {
    assert(IS_RENDER_THREAD());
    std::vector<SkinnedModelVertex> mSkinnedVerts;
    std::vector<uint16_t> mIndices;

    const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        pError("No mesh to process in this file: " + filePath.getString());
        return false;
    }

    model.mSkinnedMesh = std::make_unique<Mesh>();
    // Read read material textures matched to material names
    std::vector<MaterialID> materialIds;
    const int materialCount = sceneLoader.scene()->GetMaterialCount();
    materialIds.resize(materialCount);
    for (int i = 0; i < materialCount; ++i) {
        FbxSurfaceMaterial* fbxMaterial = sceneLoader.scene()->GetMaterial(i);

        const nString materialName = fbxMaterial->GetName();
        materialIds[i] = materialRepo.getMaterialId(materialName);
    }

    for (int m = 0; m < 1; ++m) {
        // This is not correct lol
        const MaterialID materialId = glm::min((MaterialID)m, (MaterialID)(materialIds.size() - 1));

        FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(m);

        Mesh& outMesh = *model.mSkinnedMesh;

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

                myVert.materialId = materialIds[materialId];
                f32v2 uvsFloat{ part.uvs[(int)i * 2], part.uvs[(int)i * 2 + 1] };
                assert(uvsFloat.x >= 0.0f && uvsFloat.x <= 1.0f && uvsFloat.y >= 0.0f && uvsFloat.y <= 1.0f);
                myVert.uvsPacked.x = (ui16)(uvsFloat.x * UINT16_MAX);
                myVert.uvsPacked.y = (ui16)(uvsFloat.y * UINT16_MAX);
                const f32v3* normals = (const f32v3*)(&part.normals[(int)(i * 3)]);
                myVert.normalPacked = Pack_INT_2_10_10_10_REV(normals->x, normals->y, normals->z, 0.0f);
                const f32v3* tangents = (const f32v3*)(&part.tangents[(int)(i * 3)]);
                myVert.tangentPacked = Pack_INT_2_10_10_10_REV(tangents->x, tangents->y, tangents->z, 0.0f);

                if (part.colors.size()) {
                    memcpy(&myVert.color, &part.colors[(int)(i * 4)], sizeof(uint8_t) * 4);
                }
                else {
                    myVert.color = COLOR_WHITE;
                }
                // TODO: Shrink to ui8?
                int influencesCount = part.influences_count();
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
            outMesh.mSkeletonData = std::make_unique<MeshSkeletonData>();
            outMesh.mSkeletonData->mJointRemaps = std::unique_ptr<ui8[]>(new ui8[numJoints]);
            outMesh.mSkeletonData->mInverseBindPoses = std::unique_ptr<ozz::math::Float4x4[]>(new ozz::math::Float4x4[numJoints]);
            for (int i = 0; i < numJoints; ++i) {
                outMesh.mSkeletonData->mJointRemaps[i] = (ui8)outputMesh.joint_remaps[i];
                outMesh.mSkeletonData->mInverseBindPoses[i] = outputMesh.inverse_bind_poses[i];
            }
            outMesh.mSkeletonData->mNumJoints = numJoints;

            // Get indices
            mIndices.resize(outputMesh.triangle_indices.size());
            for (int i = 0; i < outputMesh.triangle_index_count(); ++i) {
                mIndices[i] = outputMesh.triangle_indices[i];
            }

            // Upload mesh data
            MeshGpuData* meshData = &model.mSkinnedMesh->mMainMesh;

            PreciseTimer uploadTimer;
            MeshBuilderCommon::initMeshBuffers(*meshData, nullptr);
            MeshBuilderCommon::optimizeMeshAndGenerateLODs(*meshData, mIndices, mSkinnedVerts);
            MeshBuilderCommon::uploadIndexData(*meshData, mIndices.data(), mIndices.size(), 0);
            MeshBuilderCommon::uploadVertexData(*meshData, mSkinnedVerts.data(), mSkinnedVerts.size(), sizeof(SkinnedModelVertex), 0);
            //MeshBuilderCommon::uploadStandardTextureUboData(*meshData, f32v3(0.0f), textures, 0);
            meshData->mVertexType = SkinnedModelVertex::bindVertexAttribs(meshData->mVao);
            checkGlError("ModelMeshBuilder::buildStaticMeshesForModel");

            LOG_TRACE("  Upload data in {} ms", uploadTimer.stop());

            mSkinnedVerts.clear();
        }
    }


    // Find and load textures
    // Right now, textures are shared with every mesh in the scene
    // TODO: Allow different textures per submesh?
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();

    // TODO: FIX
    //const SubTexture& texture = textureRepo.getSubTextureOLD(modelFileNameNoExtension);
    //for (int i = 0; i < numMeshes; ++i) {
    //    model.mSkinnedMeshes[i].setDiffuseTexture(texture.mTextureAlbedo);
    //}
    //for (int i = 0; i < numMeshes; ++i) {
    //    model.mSkinnedMeshes[i].setNormalTexture(texture.mTextureNormal);
    //}

    //// TODO: Store this
    //VGTexture tex = textureRepo.getSubTextureOLD(modelFileNameNoExtension + ".spec").mTextureAlbedo;
    //for (int i = 0; i < numMeshes; ++i) {
    //    model.mSkinnedMeshes[i].setSpecularTexture(tex);
    //}

    model.mNumSkinningMatrices = model.getMesh()->tryGetSkeleton()->mNumJoints;

    glBindVertexArray(0);
    return true;
}

MeshCpuData ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(RawSubMesh& subMesh, const std::vector<RawMaterialData>& rawMaterials, const MaterialRepository& materialRepo) {

    MeshCpuData rv;

    // Optimize + LOD
    OptimizedCpuMeshData meshData = MeshBuilderCommon::optimizeMeshAndGenerateLODs(subMesh.mIndices, subMesh.mVertices);
    rv.mLodData = meshData.lodData;

    // Grab material IDs from material names
    std::vector<MaterialID> materialIds;
    materialIds.resize(rawMaterials.size());
    for (int i = 0; i < rawMaterials.size(); ++i) {
        const nString materialName = rawMaterials[i].materialName;
        materialIds[i] = materialRepo.getMaterialId(materialName);
    }

    // Different vertex format based on skin or no
    rv.mVertsCount = meshData.vertices.size();
    if (subMesh.mHasSkin) {
        SkinnedModelVertex* verts = new SkinnedModelVertex[rv.mVertsCount];
        rv.mVertsPtr = verts;
        rv.mVertexType = VertexType::SKINNED_MODEL;
        for (int i = 0; i < rv.mVertsCount; ++i) {
            const RawMeshVertex& rawVert = meshData.vertices[i];
            SkinnedModelVertex& myVert = verts[i];
            myVert.pos = rawVert.pos;
            myVert.materialId = materialIds[rawVert.materialIndex];
            assert(rawVert.uvs.x >= 0.0f && rawVert.uvs.x <= 1.0f && rawVert.uvs.y >= 0.0f && rawVert.uvs.y <= 1.0f);
            myVert.uvsPacked.x = (ui16)(rawVert.uvs.x * UINT16_MAX);
            myVert.uvsPacked.y = (ui16)(rawVert.uvs.y * UINT16_MAX);
            myVert.normalPacked = Pack_INT_2_10_10_10_REV(rawVert.normal.x, rawVert.normal.y, rawVert.normal.z, 0.0f);
            myVert.tangentPacked = Pack_INT_2_10_10_10_REV(rawVert.tangent.x, rawVert.tangent.y, rawVert.tangent.z, 0.0f);
            myVert.color = rawVert.color;
            memcpy(myVert.boneWeights, rawVert.boneWeights, sizeof(f32) * MAX_BONES_PER_VERTEX);
            memcpy(myVert.boneIDs, rawVert.boneIDs, sizeof(ui8) * MAX_BONES_PER_VERTEX);
        }
    }
    else {
        StaticModelVertex* verts = new StaticModelVertex[rv.mVertsCount];
        rv.mVertsPtr = verts;
        rv.mVertexType = VertexType::STATIC_MODEL;
        for (int i = 0; i < rv.mVertsCount; ++i) {
            const RawMeshVertex& rawVert = meshData.vertices[i];
            StaticModelVertex& myVert = verts[i];
            myVert.pos = rawVert.pos;
            myVert.materialId = materialIds[rawVert.materialIndex];
            assert(rawVert.uvs.x >= 0.0f && rawVert.uvs.x <= 1.0f && rawVert.uvs.y >= 0.0f && rawVert.uvs.y <= 1.0f);
            myVert.uvsPacked.x = (ui16)(rawVert.uvs.x * UINT16_MAX);
            myVert.uvsPacked.y = (ui16)(rawVert.uvs.y * UINT16_MAX);
            myVert.normalPacked = Pack_INT_2_10_10_10_REV(rawVert.normal.x, rawVert.normal.y, rawVert.normal.z, 0.0f);
            myVert.tangentPacked = Pack_INT_2_10_10_10_REV(rawVert.tangent.x, rawVert.tangent.y, rawVert.tangent.z, 0.0f);
            myVert.color = rawVert.color;
        }
    }

    // Copy indices
    // TODO: Allow int?
    rv.mIndexType = MeshIndexType::SHORT;
    ui16* indices = new ui16[meshData.indices.size()];
    rv.mElementsPtr = (void*)indices;
    for (int i = 0; i < meshData.indices.size(); ++i) {
        assert(meshData.indices[i] <= UINT16_MAX);
        indices[i] = (ui16)meshData.indices[i];
    }

    return rv;
}

void ModelMeshBuilder::uploadCpuMeshToGpu(const MeshCpuData& cpuMesh, MeshGpuData& outGpuMesh) {
    MeshBuilderCommon::initMeshBuffers(outGpuMesh, nullptr);
    assert(cpuMesh.mIndexType == MeshIndexType::SHORT); // TODO: Support int
    outGpuMesh.mLODData = cpuMesh.mLodData;
    MeshBuilderCommon::uploadIndexData(outGpuMesh, (ui16*)cpuMesh.mElementsPtr, cpuMesh.mLodData.mTotalIndexCount, 0);
    MeshBuilderCommon::uploadVertexData(outGpuMesh, cpuMesh.mVertsPtr, cpuMesh.mVertsCount, getVertexSize(cpuMesh.mVertexType), 0);
    // TODO: Common util?
    VertexType mappedVertexType = VertexType::INVALID;
    switch (cpuMesh.mVertexType) {
        case VertexType::STANDARD:
            mappedVertexType = StandardVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::TERRAIN:
            mappedVertexType = TerrainVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::WATER:
            mappedVertexType = WaterVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::STATIC_MODEL:
            mappedVertexType = StaticModelVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        case VertexType::SKINNED_MODEL:
            mappedVertexType = SkinnedModelVertex::bindVertexAttribs(outGpuMesh.mVao);
            break;
        default:
            assert(false);
    }
    static_assert(e_cast(VertexType::COUNT) == 6);
    assert(mappedVertexType == cpuMesh.mVertexType);
    checkGlError("ModelMeshBuilder::uploadCpuMeshToGpu");
}
