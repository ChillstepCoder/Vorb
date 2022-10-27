#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/model/Model3D.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>

#include "fbx/ozzFbxToMesh.hpp"

ModelRepository::ModelRepository(vio::IOManager& ioManager, vg::TextureCache& textureCache, const RigRepository& rigRepository) : mIoManager(ioManager), mTextureCache(textureCache), mRigRepository(rigRepository) {

}

ModelRepository::~ModelRepository() {

}

// TODO: Cache model files in binary
bool ModelRepository::loadModelFile(const vio::Path& filePath, const AnimMachineRepository& animMachineRepository) {
    ModelDef& def = mModelDefs.emplace_back();
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    PreciseTimer timer;

    LOG_TRACE("Loading FBX {}", filePath.getCString());

    ModelDefFileData fileData;
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, filePath, &KEG_GLOBAL_TYPE(ModelDefFileData))) {
        pError("Failed to load model file " + filePath.getString());
        return false;
    }

    if (fileData.mModelName.empty()) {
        pError("Model file missing model name " + filePath.getString());
        return false;
    }

    vio::Path rootDir = filePath;
    nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    vio::Path modelPath = rootDir + nString("\\") + fileData.mModelName;
    LOG_TRACE("  Parsed in {} ms", timer.stop());
    timer.start();

    // Load ozz Skeleton
    assert(fileData.mRigName.size());
    def.mRig = &mRigRepository.getRigDef(fileData.mRigName);
    
    // Hookup animation machine
    if (fileData.mMachineName.size()) {
        const AnimMachineDef* animMachineDef = animMachineRepository.tryGetAnimMachineDef(fileData.mMachineName);
        if (!animMachineDef) {
            pError("Failed to find anim machine " + fileData.mMachineName + " for: " + filePath.getString());
            return false;
        }
        def.mAnimMachine = animMachineDef;
    }

    // Import Fbx content.
    ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    ozz::animation::offline::fbx::FbxDefaultIOSettings settings(fbxManager);
    ozz::animation::offline::fbx::FbxSceneLoader sceneLoader((const char*)modelPath.getCString(), "", fbxManager, settings);
    if (!sceneLoader.scene()) {
        pError("Failed to import fbx scene: " + filePath.getString());
        return false;
    }
    LOG_TRACE("  Import in {} ms", timer.stop());
    timer.start();

    const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        pError("No mesh to process in this file: " + filePath.getString());
        return false;
    }

    //{  // Clean and triangulates the scene.
    //    FbxGeometryConverter converter(fbxManager);
    //    converter.RemoveBadPolygonsFromMeshes(sceneLoader.scene());
    //    std::cout << "CLEAN " << timer.stop() << " ms" << std::endl; timer.start();
    //    if (!converter.Triangulate(sceneLoader.scene(), true)) {
    //        pError("Failed to triangulate meshes: " + filePath.getString());
    //        return false;
    //    }
    //    std::cout << "TRIANGULATE " << timer.stop() << " ms" << std::endl; timer.start();
    //}

    // Copy all meshes
    SkinnedModel3D& model = def.mModel;
    model.mMeshes = std::unique_ptr<SkinnedMesh[]>(new SkinnedMesh[numMeshes]);
    model.mNumMeshes = numMeshes;
    /* ozz::vector<ozz::sample::Mesh> meshes;
     meshes.resize(numMeshes);*/
    std::vector<SkinnedModelVertex> verts;
    for (int m = 0; m < numMeshes; ++m) {
        FbxMesh* mesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(m);

        // Allocates output mesh.
        ozzfbx::Mesh outputMesh;
        outputMesh.parts.resize(1);

        ControlPointsRemap remap;
        if (!BuildVertices(mesh, sceneLoader.converter(), &remap, &outputMesh)) {
            pError("Failed to read vertices: " + filePath.getString());
            return false;
        }

        // Finds skinning informations
        if (mesh->GetDeformerCount(FbxDeformer::eSkin) > 0) {
            if (!BuildSkin(mesh, sceneLoader.converter(), remap, def.mRig->mSkeleton, &outputMesh)) {
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


            SkinnedMesh& myMesh = model.mMeshes[m];
            verts.resize(outputMesh.vertex_count());
            assert(outputMesh.parts.size() == 1);
            for (int i = 0; i < outputMesh.vertex_count(); ++i) {
                const ozzfbx::Mesh::Part& part = outputMesh.parts[0];
                SkinnedModelVertex& myVert = verts[i];
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
                memset(myVert.boneWeights, 0, sizeof(f32)* MAX_BONES_PER_VERTEX);
                if (influencesCount == 1) {
                    myVert.boneWeights[0] = 1.0f;
                }
                else {
                    memcpy(myVert.boneWeights, &part.joint_weights[(int)(i * (influencesCount - 1))], sizeof(f32) * f32(influencesCount - 1));
                }
            }
            const int numJoints = outputMesh.num_joints();
            assert(numJoints < 100);
            myMesh.mJointRemaps = std::unique_ptr<ui8[]>(new ui8[numJoints]);
            myMesh.mInverseBindPoses = std::unique_ptr<ozz::math::Float4x4[]>(new ozz::math::Float4x4[numJoints]);
            for (int i = 0; i < numJoints; ++i) {
                myMesh.mJointRemaps[i] = (ui8)outputMesh.joint_remaps[i];
                myMesh.mInverseBindPoses[i] = outputMesh.inverse_bind_poses[i];
            }
            myMesh.mNumJoints = numJoints;
            myMesh.setData(verts.data(), (ui32)verts.size(), MeshDrawMode::STATIC);
            myMesh.setIndices(outputMesh.triangle_indices.data(), outputMesh.triangle_index_count());
            LOG_TRACE("  Copy data in {} ms", timer.stop());
            timer.start();
        }
    }

    // Find and load textures
    // Right now, textures are shared with every mesh in the scene
    vio::Path textureDir = rootDir + vio::Path("\\") + vio::Path(modelFileNameNoExtension) + vio::Path(".fbm");
    if (textureDir.isDirectory()) {
        vio::Path textureNameRoot = textureDir + vio::Path("\\") + vio::Path(modelFileNameNoExtension) + vio::Path("_");

        vio::Path diffusePath = textureNameRoot + vio::Path("diffuse.png");
        if (diffusePath.isValid()) {
            vg::Texture tex = mTextureCache.addTexture(diffusePath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, true);
            for (int i = 0; i < numMeshes; ++i) {
                model.mMeshes[i].setDiffuseTexture(tex.id);
            }
        }

        vio::Path normalPath = textureNameRoot + vio::Path("normal.png");
        if (normalPath.isValid()) {
            vg::Texture tex = mTextureCache.addTexture(normalPath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, true);
            for (int i = 0; i < numMeshes; ++i) {
                model.mMeshes[i].setNormalTexture(tex.id);
            }
        }

        vio::Path specularPath = textureNameRoot + vio::Path("specular.png");
        if (specularPath.isValid()) {
            vg::Texture tex = mTextureCache.addTexture(specularPath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, true);
            for (int i = 0; i < numMeshes; ++i) {
                model.mMeshes[i].setSpecularTexture(tex.id);
            }
        }
    }

    ui8 numSkinningMatrices = 0;
    for (ui32 i = 0; i < model.getNumMeshes(); ++i) {
        numSkinningMatrices = std::max(numSkinningMatrices, model.getMeshes()[i].getNumJoints());
    }
    model.mNumSkinningMatrices = numSkinningMatrices;
    
    // Store lookup
    assert(mModelIdLookup.find(modelFileNameNoExtension) == mModelIdLookup.end());
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    return true;
}

const ModelDef& ModelRepository::getModelDef(const nString& name) const
{
    auto&& it = mModelIdLookup.find(name);
    assert(it != mModelIdLookup.end());
    return mModelDefs[it->second];
}
