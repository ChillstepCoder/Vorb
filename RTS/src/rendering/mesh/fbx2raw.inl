#pragma once


typedef std::vector<ui32> ControlPointRemap;
typedef std::vector<ControlPointRemap> ControlPointsRemap;

namespace fbx2raw {

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

    bool buildRawSubmesh(FbxMesh* fbxMesh,
        ozz::animation::offline::fbx::FbxSystemConverter* _converter,
        ControlPointsRemap* controlPointsRemap,
        RawSubMesh& subMesh,
        const std::vector<RawMaterialData>& materials
    ) {
        // This function treat all layers like if they were using mapping mode
        // eByPolygonVertex. This allow to use a single code path for all mapping
        // modes. It requires one more pass (compare to eByControlPoint mode), which
        // is to weld vertices with identical positions, normals, uvs...

        // Allocates control point to polygon remapping.
        const int controlPointCount = fbxMesh->GetControlPointsCount();
        controlPointsRemap->resize(controlPointCount);

        // Get the mesh node's transformation matrix
        FbxAMatrix transformMatrix = fbxMesh->GetNode()->EvaluateGlobalTransform();

        // Regenerate normals if they're not available.
        if (!fbxMesh->GenerateNormals(false,     // overwrite
            true,      // by ctrl point
            false)) {  // clockwise
            return false;
        }

        // Get material information
        ui32 materialIndex = 0;
        const FbxGeometryElementMaterial* elementMaterial = fbxMesh->GetElementMaterial();
        if (elementMaterial) {
            const ui32 idx = elementMaterial->GetIndexArray()[0];
            const char* materialName = fbxMesh->GetNode()->GetMaterial(idx)->GetName();
            for (size_t i = 0; i < materials.size(); ++i) {
                if (materials[i].materialName == materialName) {
                    materialIndex = i;
                    break;
                }
            }
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
        /*  if (fbxMesh->GetElementTangentCount() > 0) {
              LOG_WARN("{} HAS TANGENTS", fbxMesh->GetName());
          }
          else {
              LOG_CRITICAL("{} NO TANGENTS", fbxMesh->GetName());
          }*/
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

        // Checks materials availability
        const FbxGeometryElementMaterial* element_materials = nullptr;
        if (fbxMesh->GetElementMaterialCount() > 0) {
            element_materials = fbxMesh->GetElementMaterial(0);
        }

        // Computes worst vertex count case. Needs to allocate 3 vertices per polygon,
        // as they should all be triangles.
        const int polygonCount = fbxMesh->GetPolygonCount();
        const int vertexCount = fbxMesh->GetPolygonCount() * 3;

        // Reserve vertex buffers. Real size is unknown as redundant vertices will be
        // rejected.
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

                // BEN TMP TEST
                //const ozz::math::Float4x4 AXIS_CONVERT = ozz::math::Float4x4::FromAxisAngle(ozz::math::simd_float4::Load(1.0f, 0.0f, 0.0f, 0.0f), ozz::math::simd_float4::Load(M_PI_2, 0.0f, 0.0f, 0.0f));
                //const ozz::math::SimdFloat4 p_in = ozz::math::simd_float4::Load(
                //    static_cast<float>(position.x), static_cast<float>(position.y),
                //    static_cast<float>(position.z), 1.f);
                //// AXIS CONVERT
                //const ozz::math::SimdFloat4 p_out = AXIS_CONVERT * p_in;
                //ozz::math::Store3PtrU(p_out, &position.x);

                FbxVector4 positionVec(fbxMesh->GetControlPoints()[controlPoint]);
                positionVec = transformMatrix.MultT(positionVec);

                // Convert to ozz
                ozz::math::Float3 position = _converter->ConvertPoint(positionVec);

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


                // Push vertex data with zero initialization so we zero padding (important for meshoptimizer)
                RawMeshVertex& vertex = subMesh.mVertices.emplace_back(RawMeshVertex{});
                vertex.pos = f32v3(position.x, position.y, position.z);
                vertex.normal = f32v3(normal.x, normal.y, normal.z);
                vertex.materialIndex = materialIndex;
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

                // Check for vertex redundancy
                int redundantWith = -1;
                for (size_t r = 0; r < remap.size(); ++r) {
                    const ui32 toTest = remap[r];
                    if (memcmp(&subMesh.mVertices[toTest], &subMesh.mVertices.back(), sizeof(RawMeshVertex)) == 0) {
                        redundantWith = toTest;
                        break;
                    }
                }

                if (redundantWith >= 0) {
                    // Reuse existing vertex.
                    subMesh.mIndices[p * 3 + v] = static_cast<ui32>(redundantWith);
                    subMesh.mVertices.pop_back();
                }
                else {
                    // Deduce this vertex offset in the output vertex buffer.
                    const ui32 vertexIndex = static_cast<ui32>(subMesh.mVertices.size() - 1);

                    // Build triangle indices.
                    subMesh.mIndices[p * 3 + v] = vertexIndex;

                    // Stores vertex offset in the output vertex buffer.
                    remap.push_back(vertexIndex);
                }
            }
        }

        // Sorts triangle indices to optimize vertex cache.
        //std::qsort(array_begin(outputMesh->triangle_indices),
        //    outputMesh->triangle_indices.size() / 3, sizeof(uint16_t) * 3,
        //    &SortTriangles);

        return true;
    }


    f32v4 parseMaterialPropertyToVec4(const FbxProperty& prop) {
        f32v4 rv;
        if (prop.GetPropertyDataType().GetType() == eFbxDouble3) {
            FbxDouble3 double3(prop.Get<FbxDouble3>());
            rv.x = double3[0];
            rv.y = double3[1];
            rv.z = double3[2];
            rv.w = 1.0f;
        }
        else if (prop.GetPropertyDataType().GetType() == eFbxDouble4) {
            FbxDouble4 double4(prop.Get<FbxDouble4>());
            rv.x = double4[0];
            rv.y = double4[1];
            rv.z = double4[2];
            rv.w = double4[3];
        }
        else {
            LOG_CRITICAL("Unhandled fbx material vec property type {}", (int)prop.GetPropertyDataType().GetType());
            assert(false && "Unhandled material property vec type");
            return f32v4(0.0f);
        }
        return rv;
    }

    f32 parseMaterialPropertyToFloat(const FbxProperty& prop) {
        if (prop.GetPropertyDataType().GetType() == eFbxDouble) {
            return (f32)prop.Get<FbxDouble>();
        }
        else if (prop.GetPropertyDataType().GetType() == eFbxFloat) {
            return (f32)prop.Get<FbxFloat>();
        }
        else {
            LOG_CRITICAL("Unhandled fbx material float property type {}", (int)prop.GetPropertyDataType().GetType());
            assert(false && "Unhandled material property float type");
            return 0.0f;
        }
    }

    RawMaterialData readFbxMaterial(FbxSurfaceMaterial& fbxMaterial) {
        RawMaterialData rv;
        rv.materialName = fbxMaterial.GetName();

        //LOG_DEBUG("Material name {} ",fbxMaterial.GetName());
        for (FbxProperty matProp = fbxMaterial.GetFirstProperty(); matProp.IsValid(); matProp = fbxMaterial.GetNextProperty(matProp)) {
            FbxString strName = matProp.GetName();
            const char* propName = strName.Buffer();
            if (strcmp(propName, "EmissiveColor") == 0) {
                rv.emissiveColor = parseMaterialPropertyToVec4(matProp);
            }
            else if (strcmp(propName, "DiffuseColor") == 0) {
                rv.albedoColor = parseMaterialPropertyToVec4(matProp);
            }
            else if (strcmp(propName, "SpecularColor") == 0) {
                rv.specularColor = parseMaterialPropertyToVec4(matProp);
            }
            else if (strcmp(propName, "EmissiveFactor") == 0) {
                rv.metallicFactor = parseMaterialPropertyToFloat(matProp);
            }
            else if (strcmp(propName, "DiffuseFactor") == 0) {
                rv.albedoFactor = parseMaterialPropertyToFloat(matProp);
            }
            else if (strcmp(propName, "BumpFactor") == 0) {
                rv.bumpFactor = parseMaterialPropertyToFloat(matProp);
            }
            else if (strcmp(propName, "TransparencyFactor") == 0) {
                rv.transparencyFactor = parseMaterialPropertyToFloat(matProp);
            }

            //LOG_DEBUG("  Property {} {} {}", propName, (int)matProp.GetPropertyDataType().GetType(), matProp.GetPropertyDataType().GetName());
        }
        return rv;
    }



    // Define a per vertex skin attributes mapping.
    struct SkinMapping {
        ui8 index;
        float weight;
    };

    typedef ozz::vector<SkinMapping> SkinMappings;
    typedef ozz::vector<SkinMappings> VertexSkinMappings;

    // Sort highest weight first.
    bool SortInfluenceWeights(const SkinMapping& l, const SkinMapping& r) {
        return l.weight > r.weight;
    }

    struct strLess {
        bool operator()(const char* const& _left, const char* const& _right) const {
            return strcmp(_left, _right) < 0;
        }
    };

    bool buildSkin(
        FbxMesh* fbxMesh,
        ozz::animation::offline::fbx::FbxSystemConverter* converter,
        const ControlPointsRemap& inputRemap,
        const ozz::animation::Skeleton& skeleton,
        RawSubMesh& subMesh
    ) {

        const int skin_count = fbxMesh->GetDeformerCount(FbxDeformer::eSkin);
        if (skin_count == 0) {
            LOG_CRITICAL("No skin found for fbx mesh");
            return false;
        }
        if (skin_count > 1) {
            LOG_WARN("More than one skin found for fbx mesh, only first will be processed");
        }

        // Get skinning indices and weights.
        FbxSkin* deformer = static_cast<FbxSkin*>(fbxMesh->GetDeformer(0, FbxDeformer::eSkin));

        FbxSkin::EType skinning_type = deformer->GetSkinningType();
        if (skinning_type != FbxSkin::eRigid && skinning_type != FbxSkin::eLinear) {
            LOG_CRITICAL("Unsupported skinning type for fbx mesh");
            return false;
        }

        const ui32 numJoints = skeleton.num_joints();
        if (numJoints > UINT8_MAX) {
            LOG_CRITICAL("More than 256 joints in fbx, please reduce");
            return false;
        }

        // Builds joints names map
        typedef std::map<const char*, ui8, strLess> JointsMap;
        JointsMap jointsMap;
        for (int i = 0; i < numJoints; ++i) {
            jointsMap[skeleton.joint_names()[i]] = static_cast<ui8>(i);
        }

        // Tmp vectors while we build and minimize the joints
        std::vector<ozz::math::Float4x4> inverseBindPoses(numJoints, ozz::math::Float4x4::identity());

        // Resize to the number of vertices
        const size_t vertexCount = subMesh.mVertices.size();
        VertexSkinMappings vertexSkinMappings(vertexCount);

        // Computes geometry matrix.
        const FbxAMatrix geometry_matrix(
            fbxMesh->GetNode()->GetGeometricTranslation(FbxNode::eSourcePivot),
            fbxMesh->GetNode()->GetGeometricRotation(FbxNode::eSourcePivot),
            fbxMesh->GetNode()->GetGeometricScaling(FbxNode::eSourcePivot)
        );

        const int clusterCount = deformer->GetClusterCount();
        for (int cl = 0; cl < clusterCount; ++cl) {
            const FbxCluster* cluster = deformer->GetCluster(cl);
            const FbxNode* node = cluster->GetLink();
            if (!node) {
                LOG_ERROR("No node linked to cluster {} in fbx file.", cluster->GetName());
                continue;
            }

            const FbxCluster::ELinkMode mode = cluster->GetLinkMode();
            if (mode != FbxCluster::eNormalize) {
                LOG_CRITICAL("Unsupported link mode for joint {} in fbx file.", node->GetName());
                return false;
            }

            // Get corresponding joint index;
            JointsMap::const_iterator it = jointsMap.find(node->GetName());
            if (it == jointsMap.end()) {
                LOG_CRITICAL("Required joint {} not found in provided skeleton in fbx file.", node->GetName());
                return false;
            }
            const uint16_t joint = it->second;

            // Computes joint's inverse bind-pose matrix.
            FbxAMatrix transform_matrix;
            cluster->GetTransformMatrix(transform_matrix);
            transform_matrix *= geometry_matrix;

            FbxAMatrix transform_link_matrix;
            cluster->GetTransformLinkMatrix(transform_link_matrix);

            const FbxAMatrix inverseBindPose = transform_link_matrix.Inverse() * transform_matrix;

            // Stores inverse transformation.
            inverseBindPoses[joint] = converter->ConvertMatrix(inverseBindPose);

            // Affect joint to all vertices of the cluster.
            const int ctrlPointIndexCount = cluster->GetControlPointIndicesCount();

            const int* ctrlPointIndices = cluster->GetControlPointIndices();
            const double* ctrlPointWeights = cluster->GetControlPointWeights();
            for (int cpi = 0; cpi < ctrlPointIndexCount; ++cpi) {
                // It happens that weight is 0. In this case give the vertex a very small
                // weight so normalization succeeds.
                const float ctrlPointWeight = static_cast<float>(ctrlPointWeights[cpi]);
                const SkinMapping mapping = { joint, ctrlPointWeight == 0.f ? 1e-9f : ctrlPointWeight };

                // remap.size() can be 0, skinned control point might not be used by any
                // polygon of the mesh. Sometimes, the mesh can have less points than at
                // the time of the skinning because a smooth operator was active when
                // skinning but has been deactivated during export.
                const int ctrl_point = ctrlPointIndices[cpi];
                const ControlPointRemap& remap = inputRemap[ctrl_point];
                for (size_t v = 0; v < remap.size(); ++v) {
                    vertexSkinMappings[remap[v]].push_back(mapping);
                }
            }
        }

        // Sort joint indexes according to weights.
        // Also deduce max number of indices per vertex.
        size_t maxInfluences = 0;
        for (size_t i = 0; i < vertexCount; ++i) {
            VertexSkinMappings::reference inv = vertexSkinMappings[i];

            // Updates max_influences.
            maxInfluences = ozz::math::Max(maxInfluences, inv.size());

            // Normalize weights.
            float sum = 0.f;
            for (size_t j = 0; j < inv.size(); ++j) {
                sum += inv[j].weight;
            }
            const float inv_sum = 1.f / (sum != 0.f ? sum : 1.f);
            for (size_t j = 0; j < inv.size(); ++j) {
                inv[j].weight *= inv_sum;
            }

            // Sort weights, bigger ones first, so that lowest one can be filtered out.
            std::sort(inv.begin(), inv.end(), &SortInfluenceWeights);
        }

        // Allocates indices and weights.
        std::vector<ui8> jointIndices(vertexCount * maxInfluences);
        std::vector<float> jointWeights(vertexCount * maxInfluences);

        // Build output vertices data.
        bool vertexIsntInfluenced = false;
        for (size_t i = 0; i < vertexCount; ++i) {
            VertexSkinMappings::const_reference inv = vertexSkinMappings[i];
            ui8* indices = &jointIndices[i * maxInfluences];
            float* weights = &jointWeights[i * maxInfluences];

            // Stores joint's indices and weights.
            size_t influenceCount = inv.size();
            if (influenceCount == 0) {
                vertexSkinMappings[i].push_back({ 0, 1.f });
                influenceCount = 1;
            }

            if (influenceCount > 0) {
                size_t j = 0;
                for (; j < influenceCount; ++j) {
                    indices[j] = inv[j].index;
                    weights[j] = inv[j].weight;
                }
            }
            else {
                // No joint influencing this vertex.
                vertexIsntInfluenced = true;
            }

            // Set unused indices and weights.
            for (size_t j = influenceCount; j < maxInfluences; ++j) {
                indices[j] = 0;
                weights[j] = 0.f;
            }
        }

        if (vertexIsntInfluenced) {
            LOG_WARN("At least one vertex isn't influenced by any joints in fbx. It's been reassigned to root joint.");
        }

        assert(maxInfluences > 0 && "Vertices are not set up with any bone weights");

        // Remove lowest weight influences if needed
        if (maxInfluences > MAX_BONES_PER_VERTEX) {

            // Iterate all vertices to remove unwanted weights and renormalizes.
            // Note that weights are already sorted, so the last ones are the less
            // influencing.
            const size_t vertexCount = subMesh.mVertices.size();
            for (size_t i = 0, offset = 0; i < vertexCount; ++i, offset += MAX_BONES_PER_VERTEX) {
                // Remove exceeding influences
                float sum = 0.f;
                for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j) {
                    jointIndices[offset + j] = jointIndices[i * maxInfluences + j];
                    jointWeights[offset + j] = jointWeights[i * maxInfluences + j];
                    sum += jointWeights[offset + j];
                }
                // Renormalize weights
                for (int j = 0; j < MAX_BONES_PER_VERTEX; ++j) {
                    jointWeights[offset + j] *= 1.f / sum;
                }
            }

            // Shrink buffers
            jointIndices.resize(vertexCount * MAX_BONES_PER_VERTEX);
            jointWeights.resize(vertexCount * MAX_BONES_PER_VERTEX);
            maxInfluences = MAX_BONES_PER_VERTEX;
        }

        // Finds used joints and remaps joint indices to the minimal range.
        // The mesh might not use all skeleton joints, so this function remaps joint
        // indices to the subset of used joints. It also reorders inverse bin pose
        // matrices.
        // Collects all unique indices.
        std::vector<ui8> uniqueIndices = jointIndices;
        std::sort(uniqueIndices.begin(), uniqueIndices.end());
        uniqueIndices.erase(std::unique(uniqueIndices.begin(), uniqueIndices.end()), uniqueIndices.end());

        // Build mapping table of mesh original joints to the new ones. Unused joints
        // are set to 0.
        std::vector<ui8> originalRemap(numJoints, 0);
        for (size_t i = 0; i < uniqueIndices.size(); ++i) {
            originalRemap[uniqueIndices[i]] = static_cast<ui8>(i);
        }

        // Reset all joints in the mesh.
        for (size_t i = 0; i < jointIndices.size(); ++i) {
            jointIndices[i] = originalRemap[jointIndices[i]];
        }

        // Remaps bind poses and removes unused joints.
        for (size_t i = 0; i < uniqueIndices.size(); ++i) {
            inverseBindPoses[i] = inverseBindPoses[uniqueIndices[i]];
        }
        inverseBindPoses.resize(uniqueIndices.size());

        // Allocate and fill skeleton data
        RawMeshSkeletonData& skeletonData = subMesh.mSkeletonData;
        skeletonData.mNumJoints = uniqueIndices.size();
        skeletonData.mJointRemaps.resize(skeletonData.mNumJoints);
        skeletonData.mInverseBindPoses.resize(skeletonData.mNumJoints);
        memcpy(skeletonData.mJointRemaps.data(), uniqueIndices.data(), sizeof(ui8) * skeletonData.mNumJoints);
        memcpy(skeletonData.mInverseBindPoses.data(), inverseBindPoses.data(), sizeof(ozz::math::Float4x4) * skeletonData.mNumJoints);

        // Set all vertex data
        for (size_t i = 0; i < subMesh.mVertices.size(); ++i) {
            RawMeshVertex& myVert = subMesh.mVertices[i];
            // Make sure data is zeroed
            memset(myVert.boneIDs, 0, sizeof(ui8) * MAX_BONES_PER_VERTEX);
            memset(myVert.boneWeights, 0, sizeof(f32) * MAX_BONES_PER_VERTEX);
            // Copy data
            memcpy(myVert.boneIDs, &jointIndices[(int)(i * maxInfluences)], sizeof(ui8) * maxInfluences);
            memcpy(myVert.boneWeights, &jointWeights[(int)(i * maxInfluences)], sizeof(f32) * maxInfluences);
        }

        return true;
    };

};