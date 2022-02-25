#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "rendering/model/Model3D.h"

// TODO: REMOVE for debug dump
#include <SDL2/SDL.h>

ModelRepository::ModelRepository(vio::IOManager& ioManager, vg::TextureCache& textureCache) : mIoManager(ioManager), mTextureCache(textureCache)
{

}

ModelRepository::~ModelRepository()
{

}

bool ModelRepository::loadModelFile(const vio::Path& filePath) {
    ModelDef& def = mModelDefs.emplace_back();
    def.mModelId = mModelDefs.size() - 1u;

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
    nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    Assimp::Importer importer;
    vio::Path modelPath = rootDir + nString("\\") + def.mModelName;
    const aiScene* aiScene = importer.ReadFile(
        modelPath.getString(),
        aiProcess_GenSmoothNormals |
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
    model.mNumMeshes = aiScene->mNumMeshes;
    std::vector<ModelVertex> verts;
    for (unsigned i = 0; i < aiScene->mNumMeshes; ++i) {
        aiMesh* aiMesh = aiScene->mMeshes[i];
        assert(aiMesh->HasFaces());
        IndexedTriangleMesh& mesh = model.mMeshes[i];
        verts.resize(aiMesh->mNumVertices);
        // Copy vertex data
        for (ui32 i = 0; i < verts.size(); ++i) {
            memcpy(&verts[i].pos, &aiMesh->mVertices[i], sizeof(f32v3));
            memcpy(&verts[i].normal, &aiMesh->mNormals[i], sizeof(f32v3));
            memcpy(&verts[i].tangent, &aiMesh->mTangents[i], sizeof(f32v3));
            memcpy(&verts[i].bitangent, &aiMesh->mBitangents[i], sizeof(f32v3));
            if (aiMesh->HasVertexColors(0)) {
                verts[i].color.r = (ui8)(aiMesh->mColors[0][i].r * 255.0f);
                verts[i].color.g = (ui8)(aiMesh->mColors[0][i].g * 255.0f);
                verts[i].color.b = (ui8)(aiMesh->mColors[0][i].b * 255.0f);
                verts[i].color.a = (ui8)(aiMesh->mColors[0][i].a * 255.0f);
            }
            else {
                verts[i].color = COLOR_WHITE;
            }
            verts[i].uvs.x = aiMesh->mTextureCoords[0][i].x;
            verts[i].uvs.y = aiMesh->mTextureCoords[0][i].y;
            verts[i].uvs.z = aiMesh->mTextureCoords[0][i].z;
            verts[i].roughness = 255;
        }
        static_assert(sizeof(f32v3) == sizeof(aiVector3D));

        // Material data
        // Get textures
        aiMaterial* material = aiScene->mMaterials[aiMesh->mMaterialIndex];
        assert(material);
        aiString texPath;
        if (material->GetTextureCount(aiTextureType_DIFFUSE)) {
            vg::Texture texture = createGlTextureFromAiTexture(material, aiTextureType_DIFFUSE, texPath, aiScene);
            mesh.setDiffuseTexture(texture.id);
            //TODO: REMOVE
            //int bytesPerPage = texture.width * texture.height * sizeof(color4);
            //ui8* pixels = new ui8[texture.width * texture.height * sizeof(color4)];

            //glBindTexture(GL_TEXTURE_2D, texture.id);
            //glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            //checkGlError("Load test");
            //SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, texture.width, texture.height, 32 /*depthbytes*/, 4 * (texture.width), 0xFF, 0xFF00, 0xFF0000, 0x0);
            //SDL_SaveBMP(surface, ("KNIGHT.bmp"));
            //glBindTexture(GL_TEXTURE_2D, 0);

            //delete[] pixels;
            
        }
        if (material->GetTextureCount(aiTextureType_NORMALS)) {
            vg::Texture texture = createGlTextureFromAiTexture(material, aiTextureType_NORMALS, texPath, aiScene);
            mesh.setNormalTexture(texture.id);
            
        }
        if (material->GetTextureCount(aiTextureType_SPECULAR)) {
            vg::Texture texture = createGlTextureFromAiTexture(material, aiTextureType_SPECULAR, texPath, aiScene);
            mesh.setSpecularTexture(texture.id);
        }

        mesh.setData(verts.data(), verts.size(), MeshDrawMode::STATIC);
        mesh.setFaces(aiMesh->mFaces, aiMesh->mNumFaces);
        mesh.finishMesh(MeshDrawMode::STATIC);
    }
    assert(mModelIdLookup.find(modelFileNameNoExtension) == mModelIdLookup.end());
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
}

const ModelDef& ModelRepository::getModelDef(const nString& name)
{
    auto&& it = mModelIdLookup.find(name);
    assert(it != mModelIdLookup.end());
    return mModelDefs[it->second];
}

vg::Texture ModelRepository::createGlTextureFromAiTexture(aiMaterial* material, const aiTextureType& textureType, aiString& texPath, const aiScene* aiScene) {
    material->GetTexture(textureType, 0u, &texPath);
    const aiTexture* tex = aiScene->GetEmbeddedTexture(texPath.C_Str());
    assert(tex);
    vg::BitmapResource rs;
    if (strcmp(tex->achFormatHint, "png") == 0) {
        rs = vg::ImageIO().load((ui8*)tex->pcData, vg::ImageIOFormat::RGBA_UI8, true);
    }
    else {
        rs.width = tex->mWidth;
        rs.height = tex->mHeight;
        rs.data = tex->pcData;
    }
    assert(rs.width);
    assert(rs.height);
    assert(rs.data);
    return mTextureCache.addTexture(vio::Path(texPath.C_Str()), &rs, vg::TexturePixelType::UNSIGNED_INT_8_8_8_8_REV, vg::TextureTarget::TEXTURE_2D, &vg::SamplerState::LINEAR_CLAMP_MIPMAP, vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA);
}
