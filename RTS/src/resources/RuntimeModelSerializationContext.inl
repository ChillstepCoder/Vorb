#pragma once

constexpr ui16 RUNTIME_MODEL_SERIALIZE_VERSION = 0;
constexpr ui8 MAX_SUBMODELS = 16;

// Serializes a model in an efficient form
class RuntimeModelSerializationContext {
public:
    RuntimeModelSerializationContext(ModelDef& modelDef, const vio::Path& path) : mModelDef(modelDef), mPath(path) {}

    BINARY_SERIALIZE();
    BINARY_SERIALIZE_OUTPUT() {
        // Version
        s.value2b(RUNTIME_MODEL_SERIALIZE_VERSION);
        // Submodels
        const ui16 numMeshes = (ui16)mModelDef.mMeshes.size();
        s.value2b(numMeshes);
        for (size_t i = 0; i < numMeshes; ++i) {
            const MeshCpuData& meshData = mModelDef.mMeshes[i]->mCpuData;
            const ModelSubmeshData& submeshData = mModelDef.mMeshesModelData[i];
            // Name
            s.object(submeshData.name);
            // Render Pass
            const ui8 renderPass = e_cast(mModelDef.mMeshes[i]->getRenderPass());
            s.value1b(renderPass);
            // LOD data
            s.object(meshData.mLodData);
            // Vertices
            s.value1b((ui8)meshData.mVertexType);

            assert(meshData.mVertsPtr);
            assert(meshData.mVertsCount);
            switch (meshData.mVertexType) {
                case VertexType::STANDARD_MODEL: {
                    StandardModelVertex* ptr = static_cast<StandardModelVertex*>(meshData.mVertsPtr);
                    s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mVertsCount));
                    break;
                }
                case VertexType::SKINNED_MODEL: {
                    SkinnedModelVertex* ptr = static_cast<SkinnedModelVertex*>(meshData.mVertsPtr);
                    s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mVertsCount));
                    break;
                }
                default:
                    panic("Saving invalid vertex type {}", (int)meshData.mVertexType);
                    break;
            }
            static_assert(e_count(VertexType) == 5);
            // Indices
            assert(meshData.mElementsPtr);
            assert(meshData.mElementsCount);
            if (meshData.mIndexType == MeshIndexType::UINT) {
                s.boolValue(1); // IsUInt 
                ui32* ptr = static_cast<ui32*>(meshData.mElementsPtr);
                s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mElementsCount));
            }
            else {
                // Short
                s.boolValue(0); // IsUInt 
                ui16* ptr = static_cast<ui16*>(meshData.mElementsPtr);
                s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mElementsCount));
            }

            // Variants
            s.container(mModelDef.mVariants, (size_t)MAX_MODEL_VARIANTS);

            // Skeleton
            if (mModelDef.isSkeletalModel()) {
                s.boolValue(1); // HasSkeleton
                const MeshSkeletonData& skeletonData = mModelDef.getSkeletalMesh(i).getSkeletonData();
                s.ext(skeletonData.mJointRemaps, bitsery::ext::PodStructUniquePointerArray(skeletonData.mNumJoints));
                s.ext(skeletonData.mInverseBindPoses, bitsery::ext::PodStructUniquePointerArray(skeletonData.mNumJoints));
            }
            else {
                s.boolValue(0);  // HasSkeleton
            }
        }
    }
    BINARY_SERIALIZE_INPUT() {
        // Version
        ui16 version;
        s.value2b(version);
        if (version != RUNTIME_MODEL_SERIALIZE_VERSION) [[unlikely]] {
            panic("Tried to load model asset {} with version {} but expected {} try validating game files", mPath.getCString(), version, RUNTIME_MODEL_SERIALIZE_VERSION);
        }

        // Submodels
        ui16 numMeshes = 0;
        s.value2b(numMeshes);
        assert(numMeshes && numMeshes <= MAX_SUBMODELS);

        mModelDef.mMeshes.resize(numMeshes);
        mModelDef.mMeshesModelData.resize(numMeshes);
        mModelDef.mTotalSubmeshJointTransformsNeeded = 0;
        for (size_t i = 0; i < numMeshes; ++i) {
            if (mModelDef.isSkeletalModel()) {
                mModelDef.mMeshes[i] = std::make_unique<SkeletalMesh>();
            }
            else {
                mModelDef.mMeshes[i] = std::make_unique<Mesh>();
            }
            MeshCpuData& meshData = mModelDef.mMeshes[i]->mCpuData;
            ModelSubmeshData& submeshData = mModelDef.mMeshesModelData[i];

            mModelDef.mMeshes[i]->setSubmeshData(&submeshData);
            // Name
            s.object(submeshData.name);
            // Render Pass
            ui8 renderPass;
            s.value1b(renderPass);
            mModelDef.mMeshes[i]->setRenderPass(static_cast<MaterialRenderPassType>(renderPass));
            // LOD data
            s.object(meshData.mLodData);
            // Vertices
            ui8 vType;
            s.value1b(vType);
            meshData.mVertexType = (VertexType)vType;
            switch (meshData.mVertexType) {
                case VertexType::STANDARD_MODEL: {
                    StandardModelVertex* ptr = nullptr;
                    s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mVertsCount));
                    meshData.mVertsPtr = ptr;
                    break;
                }
                case VertexType::SKINNED_MODEL: {
                    SkinnedModelVertex* ptr = nullptr;
                    s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mVertsCount));
                    meshData.mVertsPtr = ptr;
                    break;
                }
                default:
                    panic("loading invalid vertex type {} on {}", (int)meshData.mVertexType, mPath.getCString());
                    break;
            }
            static_assert(e_count(VertexType) == 5);
            // Indices
            bool isUint;
            s.boolValue(isUint);
            if (isUint) {
                meshData.mIndexType = MeshIndexType::UINT;
                ui32* ptr = nullptr;
                s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mElementsCount));
                meshData.mElementsPtr = ptr;
            }
            else {
                meshData.mIndexType = MeshIndexType::USHORT;
                ui16* ptr = nullptr;
                s.ext(ptr, bitsery::ext::PodStructRawPointerArray(meshData.mElementsCount));
                meshData.mElementsPtr = ptr;
            }

            // Variants
            s.container(mModelDef.mVariants, (size_t)MAX_MODEL_VARIANTS);

            // Skeleton
            bool hasSkeleton;
            s.boolValue(hasSkeleton);
            if (hasSkeleton != mModelDef.isSkeletalModel()) [[unlikely]] {
                panic("Tried to read model {} mPath.getCString() marked as HAS_SKELETON:{} but expected {}", mPath.getCString(), hasSkeleton, mModelDef.isSkeletalModel());
            }

            if (hasSkeleton) {
                MeshSkeletonData& skeletonData = mModelDef.getSkeletalMesh(i).getSkeletonData();
                ui32 numRemaps;
                ui32 numBindPoses;
                s.ext(skeletonData.mJointRemaps, bitsery::ext::PodStructUniquePointerArray(numRemaps));
                s.ext(skeletonData.mInverseBindPoses, bitsery::ext::PodStructUniquePointerArray(numBindPoses));
                assert(numRemaps == numBindPoses);
                skeletonData.mNumJoints = numRemaps;
                mModelDef.mTotalSubmeshJointTransformsNeeded += skeletonData.mNumJoints;
            }
        }
    }

private:
    ModelDef& mModelDef;
    const vio::Path& mPath;
};

