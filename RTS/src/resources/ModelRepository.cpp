#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/model/Model3D.h"
#include "rendering/mesh/ModelMeshBuilder.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>

typedef std::vector<ui32> ControlPointRemap;
typedef std::vector<ControlPointRemap> ControlPointsRemap;

template <typename _Element>
bool GetElement(const _Element& _layer, int _vertex_id, int _control_point,
    typename _Element::ArrayElementType* _out) {
    assert(_out);

    int direct_array_id;
    switch (_layer.GetMappingMode()) {
        case FbxGeometryElement::eByControlPoint: {
            switch (_layer.GetReferenceMode()) {
                case FbxGeometryElement::eDirect: {
                    direct_array_id = _control_point;
                    break;
                }
                case FbxGeometryElement::eIndexToDirect: {
                    direct_array_id = _layer.GetIndexArray().GetAt(_control_point);
                    break;
                }
                default:
                    return false;  // Unhandled reference mode.
            }
            break;
        }
        case FbxGeometryElement::eByPolygonVertex: {
            switch (_layer.GetReferenceMode()) {
                case FbxGeometryElement::eDirect: {
                    direct_array_id = _vertex_id;
                    break;
                }
                case FbxGeometryElement::eIndexToDirect: {
                    direct_array_id = _layer.GetIndexArray().GetAt(_vertex_id);
                    break;
                }
                default:
                    return false;  // Unhandled reference mode.
            }
            break;
        }
        default:
            return false;  // Unhandled mapping mode.
    }

    // Extract data from the layer direct array.
    *_out = _layer.GetDirectArray().GetAt(direct_array_id);

    return true;
}

template <typename _T>
bool OzzCompare(const _T* _a, const _T* _b, size_t _count) {
    size_t i = 0;
    for (; i < _count && _a[i] == _b[i]; ++i)
        ;
    return i == _count;
}

bool buildVertices(FbxMesh* fbxMesh,
    ozz::animation::offline::fbx::FbxSystemConverter* _converter,
    ControlPointsRemap* controlPointsRemap,
    RawMesh& outputMesh) {
    // This function treat all layers like if they were using mapping mode
    // eByPolygonVertex. This allow to use a single code path for all mapping
    // modes. It requires one more pass (compare to eByControlPoint mode), which
    // is to weld vertices with identical positions, normals, uvs...

    // Allocates control point to polygon remapping.
    const int controlPointCount = fbxMesh->GetControlPointsCount();
    controlPointsRemap->resize(controlPointCount);

    // Regenerate normals if they're not available.
    if (!fbxMesh->GenerateNormals(false,     // overwrite
        true,      // by ctrl point
        false)) {  // clockwise
        return false;
    }

    assert(fbxMesh->GetElementNormalCount() > 0);
    const FbxGeometryElementNormal* element_normals =
        fbxMesh->GetElementNormal(0);
    assert(element_normals);

    // Checks uvs availability.
    const FbxGeometryElementUV* element_uvs = nullptr;
    if (fbxMesh->GetElementUVCount() > 0) {
        element_uvs = fbxMesh->GetElementUV(0);
    }

    // Checks tangents availability.
    const FbxGeometryElementTangent* element_tangents = nullptr;
    if (element_uvs) {  // UVs are needed to generate tangents.
      // Regenerate tangents if they're not available.
        if (!fbxMesh->GenerateTangentsData(0, false)) {
            return false;
        }
    }
    if (fbxMesh->GetElementTangentCount() > 0) {
        element_tangents = fbxMesh->GetElementTangent(0);
    }

    // Checks vertex colors availability.
    const FbxGeometryElementVertexColor* element_colors = nullptr;
    if (fbxMesh->GetElementVertexColorCount() > 0) {
        element_colors = fbxMesh->GetElementVertexColor(0);
    }

    // Computes worst vertex count case. Needs to allocate 3 vertices per polygon,
    // as they should all be triangles.
    const int polygonCount = fbxMesh->GetPolygonCount();
    const int vertexCount = fbxMesh->GetPolygonCount() * 3;

    // Reserve vertex buffers. Real size is unknown as redundant vertices will be
    // rejected.
    outputMesh.mSubMeshes.resize(1);
    RawSubMesh& subMesh = outputMesh.mSubMeshes[0];
    subMesh.mVertices.reserve(vertexCount);
    /*part.positions.reserve(vertexCount *
        ozzfbx::Mesh::Part::kPositionsCpnts);
    part.normals.reserve(vertexCount * ozzfbx::Mesh::Part::kNormalsCpnts);
    if (element_tangents) {
        part.tangents.reserve(vertexCount *
            ozzfbx::Mesh::Part::kTangentsCpnts);
    }
    if (element_uvs) {
        part.uvs.reserve(vertexCount * ozzfbx::Mesh::Part::kUVsCpnts);
    }
    if (element_colors) {
        part.colors.reserve(vertexCount * ozzfbx::Mesh::Part::kColorsCpnts);
    }*/

    // Resize triangle indices, as their size is known.
    subMesh.mIndices.resize(vertexCount);

    // Iterate all polygons and stores ctrl point to polygon mappings.
    int vertexId = 0;
    for (int p = 0; p < polygonCount; ++p) {
        if (fbxMesh->GetPolygonSize(p) != 3) {
            LOG_CRITICAL("ERROR: Mesh {} must have been triangulated before import", fbxMesh->GetName());
            assert(false && "Mesh must have been triangulated.");
        }

        for (int v = 0; v < 3; ++v, ++vertexId) {
            // Get control point.
            const int controlPoint = fbxMesh->GetPolygonVertex(p, v);
            assert(controlPoint >= 0);
            ControlPointRemap& remap = controlPointsRemap->at(controlPoint);

            // Get vertex position.
            const ozz::math::Float3 position = _converter->ConvertPoint(fbxMesh->GetControlPoints()[controlPoint]);

            // Get vertex normal.
            FbxVector4 src_normal(0.f, 1.f, 0.f, 0.f);
            if (!GetElement(*element_normals, vertexId, controlPoint, &src_normal)) {
                return false;
            }
            const ozz::math::Float3 normal = NormalizeSafe(
                _converter->ConvertVector(src_normal), ozz::math::Float3::y_axis());

            // Get vertex tangent.
            FbxVector4 src_tangent(1.f, 0.f, 0.f, 0.f);
            if (element_tangents) {
                if (!GetElement(*element_tangents, vertexId, controlPoint,
                    &src_tangent)) {
                    return false;
                }
            }
            const ozz::math::Float3 tangent3 = NormalizeSafe(
                _converter->ConvertVector(src_tangent), ozz::math::Float3::x_axis());
            const ozz::math::Float4 tangent(tangent3,
                static_cast<float>(src_tangent[3]));

            // Get vertex uv.
            FbxVector2 src_uv;
            if (element_uvs) {
                if (!GetElement(*element_uvs, vertexId, controlPoint, &src_uv)) {
                    return false;
                }
            }
            else {
                src_uv = FbxVector2(0., 0.);
            }
            const ozz::math::Float2 uv(static_cast<float>(src_uv[0]),
                static_cast<float>(src_uv[1]));

            // Get vertex colors.
            FbxColor src_color;
            if (element_colors) {
                if (!GetElement(*element_colors, vertexId, controlPoint, &src_color)) {
                    return false;
                }
            }
            else {
                src_color = FbxColor(1., 1., 1., 1.);
            }
            const uint8_t color[4] = {
                static_cast<uint8_t>(
                    ozz::math::Clamp(0., src_color.mRed * 255., 255.)),
                static_cast<uint8_t>(
                    ozz::math::Clamp(0., src_color.mGreen * 255., 255.)),
                static_cast<uint8_t>(
                    ozz::math::Clamp(0., src_color.mBlue * 255., 255.)),
                static_cast<uint8_t>(
                    ozz::math::Clamp(0., src_color.mAlpha * 255., 255.)),
            };

            //// Check for vertex redundancy, only with other points that share the same
            //// control point.
            //int redundantWith = -1;
            //for (size_t r = 0; r < remap.size(); ++r) {
            //    const ui32 toTest = remap[r];

            //    // Check for identical normals.
            //    if (!OzzCompare(
            //        &normal.x,
            //        &part.normals[toTest * ozzfbx::Mesh::Part::kNormalsCpnts],
            //        ozzfbx::Mesh::Part::kNormalsCpnts)) {
            //        continue;  // Next vertex.
            //    }

            //    // Check for identical uvs.
            //    if (element_uvs) {
            //        if (!OzzCompare(&uv.x,
            //            &part.uvs[toTest * ozzfbx::Mesh::Part::kUVsCpnts],
            //            ozzfbx::Mesh::Part::kUVsCpnts)) {
            //            continue;  // Next vertex.
            //        }
            //    }

            //    // Check for identical colors.
            //    if (element_colors) {
            //        if (!OzzCompare(
            //            color,
            //            &part.colors[toTest * ozzfbx::Mesh::Part::kColorsCpnts],
            //            ozzfbx::Mesh::Part::kColorsCpnts)) {
            //            continue;  // Next vertex.
            //        }
            //    }

            //    // Check for identical tangents.
            //    if (element_tangents) {
            //        if (!OzzCompare(&tangent.x,
            //            &part.tangents[toTest *
            //            ozzfbx::Mesh::Part::kTangentsCpnts],
            //            ozzfbx::Mesh::Part::kColorsCpnts)) {
            //            continue;  // Next vertex.
            //        }
            //    }

            //    // This vertex is redundant with an existing one.
            //    redundantWith = toTest;
            //    break;
            //}

            //if (redundantWith >= 0) {
            //    assert(redundantWith <= std::numeric_limits<uint16_t>::max());

            //    // Reuse existing vertex.
            //    outputMesh->triangle_indices[p * 3 + v] =
            //        static_cast<uint16_t>(redundantWith);
            //}
            ////else {
            //// Detect triangle indices overflow.
            //if ((part.positions.size() / 3) >
            //    std::numeric_limits<uint16_t>::max()) {
            //    ozz::log::Err() << "Mesh uses too many vertices (> "
            //        << std::numeric_limits<uint16_t>::max()
            //        << ") to fit in the index "
            //        "buffer."
            //        << std::endl;
            //    return false;
            //}

            // Deduce this vertex offset in the output vertex buffer.
            ui32 vertexIndex = static_cast<ui32>(subMesh.mVertices.size());

            // Build triangle indices.
            subMesh.mIndices[p * 3 + v] = vertexIndex;

            // Stores vertex offset in the output vertex buffer.
            remap.push_back(vertexIndex);

            // Push vertex data.
            RawMeshVertex& vertex = subMesh.mVertices.emplace_back();
            vertex.pos = f32v3(position.x, position.y, position.z);
            vertex.normal = f32v3(normal.x, normal.y, normal.z);
            if (element_uvs) {
                vertex.uvs = f32v2(uv.x, uv.y);
            }
            else {
                vertex.uvs = f32v2(0.0f);
            }
            if (element_tangents) {
                vertex.tangent = f32v3(tangent.x, tangent.y, tangent.z);
                // Right or left handed
                if (tangent.w > 0.0f) {
                    vertex.tangent = -vertex.tangent;
                }
            }
            else {
                vertex.tangent = f32v3(1.0f, 0.0f, 0.0f);
            }
            if (element_colors) {
                vertex.color = color4(color[0], color[1], color[2], color[3]);
            }
            else {
                vertex.color = COLOR_WHITE;
            }
            //}
        }
    }

    // Sorts triangle indices to optimize vertex cache.
    //std::qsort(array_begin(outputMesh->triangle_indices),
    //    outputMesh->triangle_indices.size() / 3, sizeof(uint16_t) * 3,
    //    &SortTriangles);

    return true;
}

ModelRepository::ModelRepository(vio::IOManager& ioManager, const RigRepository& rigRepository) : mIoManager(ioManager), mRigRepository(rigRepository) {

}

ModelRepository::~ModelRepository() {

}

// TODO: Cache model files in binary
bool ModelRepository::loadModelFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository) {

    PROFILE_FUNCTION();

    LOG_TRACE("Loading model {}", filePath.getCString());

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
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    vio::Path modelPath = rootDir + nString("\\") + fileData.mModelName;

    if (fileData.mRigName.size()) {
        // Load ozz Skeleton if we use it
        return loadSkinnedModel(fileData, materialRepository, animMachineRepository, filePath, modelPath, rootDir);
    }
    else {
        return loadStaticModel(fileData, materialRepository, filePath, modelPath, rootDir);
    }
    return false;
}

bool ModelRepository::loadFbxFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository) {
    PROFILE_FUNCTION();

    LOG_TRACE("Loading FBX {}", filePath.getCString());

    vio::Path rootDir = filePath;
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    ModelDefFileData fileData;
    return loadStaticModel(fileData, materialRepository, filePath, filePath, rootDir);
}

bool ModelRepository::loadSkinnedModel(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository, const vio::Path& filePath, const vio::Path& modelPath, const vio::Path& rootDir) {

    PROFILE_FUNCTION();

    ModelDef& def = *mModelDefs.emplace_back(std::make_unique<ModelDef>());
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    def.mModelType = Model3DType::SKINNED;
    def.mShadowDetail = fileData.mShadowDetail;

    PreciseTimer timer;
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

    SkinnedModel3D& model = def.getSkinnedModel();
    ModelMeshBuilder meshBuilder;
    meshBuilder.buildSkinnedMeshesForModel(model, def.mRig->mSkeleton, filePath, rootDir, sceneLoader, MeshDrawMode::STATIC, materialRepository);

    // Store lookup
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    assert(mModelIdLookup.find(modelFileNameNoExtension) == mModelIdLookup.end());
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    // TODO: Don't use extra lookup to copy the name?
    def.mName = mModelIdLookup.find(modelFileNameNoExtension)->first.c_str();
    return true;
}

bool ModelRepository::loadStaticModel(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const vio::Path& filePath, const vio::Path& modelPath, const vio::Path& rootDir) {

    PROFILE_FUNCTION();

    ModelDef& def = *mModelDefs.emplace_back(std::make_unique<ModelDef>());
    def.mModelType = Model3DType::STATIC;
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    def.mShadowDetail = fileData.mShadowDetail;

    PreciseTimer timer;

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

    StaticModel3D& model = def.getStaticModel();
    ModelMeshBuilder meshBuilder;
    meshBuilder.buildStaticMeshesForModel(model, filePath, rootDir, sceneLoader, MeshDrawMode::STATIC, materialRepository, fileData.mScale);

    // Store lookup
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    if (mModelIdLookup.find(modelFileNameNoExtension) != mModelIdLookup.end()) {
        LOG_INFO("Replacing model {}", filePath.getCString());
    }
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    // TODO: Don't use extra lookup to copy the name?
    def.mName = mModelIdLookup.find(modelFileNameNoExtension)->first.c_str();
    return true;
}

bool ModelRepository::loadRawModel(const vio::Path& filePath) {

    // Import Fbx content.
    ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    ozz::animation::offline::fbx::FbxDefaultIOSettings settings(fbxManager);
    ozz::animation::offline::fbx::FbxSceneLoader sceneLoader((const char*)filePath.getCString(), "", fbxManager, settings);
    if (!sceneLoader.scene()) {
        pError("Failed to import fbx scene: " + filePath.getString());
        return false;
    }


    const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        pError("No mesh to process in this file: " + filePath.getString());
        return false;
    }

    RawMesh rawFbxMesh;

    // Read read material textures matched to material names
    const int materialCount = sceneLoader.scene()->GetMaterialCount();
    rawFbxMesh.mMaterials.resize(materialCount);
    for (int i = 0; i < materialCount; ++i) {
        FbxSurfaceMaterial* fbxMaterial = sceneLoader.scene()->GetMaterial(i);
        rawFbxMesh.mMaterials[i].materialName = fbxMaterial->GetName();

         LOG_DEBUG("Material {} name {} ", i, fbxMaterial->GetName());
         for (FbxProperty matProp = fbxMaterial->GetFirstProperty(); matProp.IsValid(); matProp = fbxMaterial->GetNextProperty(matProp)) {
             LOG_DEBUG("  Property {}",  matProp.GetName().Buffer());
         }
    }

    //for (int m = 0; m < numMeshes; ++m) {

    //    FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(m);
    //    FbxLayerElementArrayTemplate<int>* pLockableArray;
    //    fbxMesh->GetMaterialIndices(&pLockableArray);
    //    LOG_DEBUG("    Material sttuff {} {}", pLockableArray->GetCount(), pLockableArray->GetFirst());

    //    PreciseTimer timer;
    //    // Allocates output mesh.
    //    ozzfbx::Mesh outputMesh;
    //    outputMesh.parts.resize(1);

    //    ControlPointsRemap remap;
    //    // TODO: Non OZZ version so we dont have an intermediate conversion
    //    if (!BuildVertices(fbxMesh, sceneLoader.converter(), &remap, &outputMesh)) {
    //        pError("Failed to read vertices: " + filePath.getString());
    //        return false;
    //    }

    //    const size_t prevSize = mStaticVerts.size();
    //    mStaticVerts.resize(mStaticVerts.size() + outputMesh.vertex_count());
    //    assert(outputMesh.parts.size() == 1);
    //    const int materialIndex = glm::min(m, (int)materialIds.size() - 1);
    //    for (int i = 0; i < outputMesh.vertex_count(); ++i) {
    //        const ozzfbx::Mesh::Part& part = outputMesh.parts[0];
    //        StaticModelVertex& myVert = mStaticVerts[prevSize + i].mStaticModel;
    //        memcpy(&myVert.pos, &part.positions[(int)(i * 3)], sizeof(f32) * 3);
    //        myVert.pos *= modelScale;
    //        myVert.materialId = materialIds[materialIndex]; // TODO: Smarter
    //        f32v2 uvsFloat{ part.uvs[(int)i * 2], part.uvs[(int)i * 2 + 1] };
    //        assert(uvsFloat.x >= 0.0f && uvsFloat.x <= 1.0f && uvsFloat.y >= 0.0f && uvsFloat.y <= 1.0f);
    //        myVert.uvsPacked.x = (ui16)(uvsFloat.x * UINT16_MAX);
    //        myVert.uvsPacked.y = (ui16)(uvsFloat.y * UINT16_MAX);
    //        const f32v3* normals = (const f32v3*)(&part.normals[(int)(i * 3)]);
    //        myVert.normalPacked = Pack_INT_2_10_10_10_REV(normals->x, normals->y, normals->z, 0.0f);
    //        const f32v3* tangents = (const f32v3*)(&part.tangents[(int)(i * 3)]);
    //        myVert.tangentPacked = Pack_INT_2_10_10_10_REV(tangents->x, tangents->y, tangents->z, 0.0f);
    //        // memcpy(&myVert.normal, &part.normals[(int)(i * 3)], sizeof(f32) * 3);
    //        // memcpy(&myVert.tangent, &part.tangents[(int)(i * 3)], sizeof(f32) * 3);
    //         // TODO: Check materials for this mesh! See if each mesh has its own material data we can leverage
    //        if (part.colors.size()) {
    //            memcpy(&myVert.color, &part.colors[(int)(i * 4)], sizeof(uint8_t) * 4);
    //        }
    //        else {
    //            myVert.color = COLOR_WHITE;
    //        }
    //    }
    //    // Copy indices
    //    size_t start = mIndices.size();
    //    mIndices.resize(mIndices.size() + outputMesh.triangle_indices.size());

    //    for (int i = 0; i < outputMesh.triangle_index_count(); ++i) {
    //        mIndices[start + i] = outputMesh.triangle_indices[i] + prevSize; // Copy index data and shift index
    //    }


    //    timer.start();
    //}



}

const ModelDef& ModelRepository::getModelDef(const nString& name) const
{
    auto&& it = mModelIdLookup.find(name);
    assert(it != mModelIdLookup.end());
    return *mModelDefs[it->second];
}

ModelID ModelRepository::getModelID(const nString& name) const {
    auto&& it = mModelIdLookup.find(name);
    if (it == mModelIdLookup.end()) {
        LOG_CRITICAL("Model {} not found", name);
        return INVALID_MODEL_ID;
    }
    return it->second;
}
