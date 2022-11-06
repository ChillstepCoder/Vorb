#include "stdafx.h"
#include "ModelMeshBuilder.h"

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
    //FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(meshIndex);

    //PreciseTimer timer;
    //// Allocates output mesh.
    //ozzfbx::Mesh outputMesh;
    //outputMesh.parts.resize(1);

    //ControlPointsRemap remap;
    //if (!BuildVertices(fbxMesh, sceneLoader.converter(), &remap, &outputMesh)) {
    //    pError("Failed to read vertices: " + filePath.getString());
    //    return false;
    //}

    //// Finds skinning informations
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

    //    mStaticVerts.resize(outputMesh.vertex_count());
    //    assert(outputMesh.parts.size() == 1);
    //    LOG_CRITICAL(" NEED TO FIX normals ModelMeshBuilder::buildStaticMesh");
    //    for (int i = 0; i < outputMesh.vertex_count(); ++i) {
    //        const ozzfbx::Mesh::Part& part = outputMesh.parts[0];
    //        StaticModelVertex& myVert = mStaticVerts[i];
    //        memcpy(&myVert.pos, &part.positions[(int)(i * 3)], sizeof(f32) * 3);
    //        
    //        // TODO: https://www.khronos.org/opengl/wiki/Normalized_Integer#Alternate_mapping
    //        // https://stackoverflow.com/questions/35961057/how-to-pack-normals-into-gl-int-2-10-10-10-rev
    //        myVert.normalPacked = {};
    //        myVert.tangentPacked = {};
    //        //memcpy(&myVert.normal, &part.normals[(int)(i * 3)], sizeof(f32) * 3);
    //        //memcpy(&myVert.tangent, &part.tangents[(int)(i * 3)], sizeof(f32) * 3);
    //        memcpy(&myVert.uvs, &part.uvs[(int)(i * 2)], sizeof(f32) * 2);
    //        if (part.colors.size()) {
    //            memcpy(&myVert.color, &part.colors[(int)(i * 4)], sizeof(uint8_t) * 4);
    //        }
    //        else {
    //            myVert.color = COLOR_WHITE;
    //        }
  

    //    }

    //    // TODO: Load joints as sockets? Not true skeleton, for attachments
    //    outMesh.setData(mSkinnedVerts.data(), (ui32)mSkinnedVerts.size(), MeshDrawMode::STATIC);
    //    outMesh.setIndices(outputMesh.triangle_indices.data(), outputMesh.triangle_index_count());
    //    mSkinnedVerts.clear();
    //    LOG_TRACE("  Copy data in {} ms", timer.stop());
    //    timer.start();
    //}
    return false;
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
}
