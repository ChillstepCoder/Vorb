#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "rendering/model/Model3D.h"

ModelRepository::ModelRepository(vio::IOManager& ioManager) : mIoManager(ioManager)
{

}

ModelRepository::~ModelRepository()
{

}

bool ModelRepository::loadModelFile(const vio::Path& filePath) {
    ModelDef def;

    if (!mIoManager.parseFileAsKegObject((ui8*)&def, filePath, &KEG_GLOBAL_TYPE(ModelDef))) {
        pError("Failed to load model file " + filePath.getString());
        return false;
    }

    if (def.mSkeletonName.empty()) {
        pError("Model file missing skeleton name " + filePath.getString());
        return false;
    }

    if (def.mModelName.empty()) {
        pError("Model file missing model name " + filePath.getString());
        return false;
    }

    vio::Path rootDir = filePath;
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    Assimp::Importer importer;
    vio::Path modelPath = rootDir + nString("\\") + def.mModelName;
    const aiScene* aiScene = importer.ReadFile(
        modelPath.getString(),
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType
    );

    if (aiScene == nullptr) {
        pError("Asset import failure - " + modelPath.getString() + " - " + importer.GetErrorString());
        return false;
    }

    assert(aiScene->HasMaterials());
    assert(aiScene->HasMeshes());

    // TODO: Materials
    /*for (unsigned i = 0; i < aiScene->mNumMaterials; ++i) {
        aiMaterial* material = aiScene->mMaterials[i];
        ui32 result = 0;
        aiColor3D out;
        material->Get(AI_MATKEY_COLOR_DIFFUSE, out);
        std::cout << out.r << " " << out.g << " " << out.b << std::endl;
    }*/
    Model3D& model = def.mModel;
    model.mMeshes = std::unique_ptr<IndexedTriangleMesh[]>(new IndexedTriangleMesh[aiScene->mNumMeshes]);
    std::vector<ModelVertex> verts;
    for (unsigned i = 0; i < aiScene->mNumMeshes; ++i) {
        aiMesh* aiMesh = aiScene->mMeshes[i];
        assert(aiMesh->HasFaces());
        IndexedTriangleMesh& mesh = model.mMeshes[i];
        verts.resize(aiMesh->mNumVertices);
        for (ui32 i = 0; i < verts.size(); ++i) {
            memcpy(&verts[i].pos, &aiMesh->mVertices[i], sizeof(f32v3));
            memcpy(&verts[i].normal, &aiMesh->mNormals[i], sizeof(f32v3));
            memcpy(&verts[i].tangent, &aiMesh->mTangents[i], sizeof(f32v3));
            if (aiMesh->HasVertexColors(0)) {
                verts[i].color.r = (ui8)(aiMesh->mColors[i][0].r * 255.0f);
                verts[i].color.g = (ui8)(aiMesh->mColors[i][0].g * 255.0f);
                verts[i].color.b = (ui8)(aiMesh->mColors[i][0].b * 255.0f);
                verts[i].color.a = (ui8)(aiMesh->mColors[i][0].a * 255.0f);
            }
            else {
                verts[i].color = COLOR_WHITE;
            }
            verts[i].uvs.x = aiMesh->mTextureCoords[i][0].x;
            verts[i].uvs.y = aiMesh->mTextureCoords[i][0].y;
            verts[i].uvs.z = aiMesh->mTextureCoords[i][0].z;
            verts[i].roughness = 255;
        }
        static_assert(sizeof(f32v3) == sizeof(aiVector3D));
        assert(false); // TEXTURES?
        mesh.setData(verts.data(), verts.size(), MeshDrawMode::STATIC);
        mesh.setFaces(aiMesh->mFaces, aiMesh->mNumFaces);
        mesh.finishMesh(MeshDrawMode::STATIC);
    }

    assert(false);
}
