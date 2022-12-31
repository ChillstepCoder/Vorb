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

            LOG_DEBUG("  Property {} {} {}", propName, (int)matProp.GetPropertyDataType().GetType(), matProp.GetPropertyDataType().GetName());
        }
        return rv;
    }
};