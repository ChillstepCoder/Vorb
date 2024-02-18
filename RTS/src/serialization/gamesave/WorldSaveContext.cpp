#include "stdafx.h"
#include "WorldSaveContext.h"

#include "world/World.h"
#include "world/host/HostWorldData.h"
// TODO: DiskIOThread
#include "serialization/GameSaveManager.h"

//https://github.com/facebook/zstd
#include <zstd.h>

const char* REGION_EXTENSION = ".rdat";

std::span<uint8_t> DeserializedRegionFileData::getPatchBytes(ui32 patchId) {
    if (!isValid()) return {};
    assert(header.patches.size() > patchId);
    auto& patch = header.patches[patchId];
    return std::span<uint8_t>(fileBytes.data() + patch.mStartByte, (size_t)patch.mAllocatedBytes);
}

WorldSaveContext::WorldSaveContext(World& world) :
    mWorld(world) {
    mRegionFileClusters[e_cast(RegionType::Height)] = RegionFileCluster(mWorld.getWidthTiles(), HEIGHTMAP_PATCH_WIDTH_TILES, HEIGHT_REGION_WIDTH_PATCHES, "height", 1024);
    mRegionFileClusters[e_cast(RegionType::Biome)] = RegionFileCluster(mWorld.getWidthTiles(), BIOME_PATCH_WIDTH_TILES, BIOME_REGION_WIDTH_PATCHES, "biome", 64);
    mRegionFileClusters[e_cast(RegionType::Chunk)] = RegionFileCluster(mWorld.getWidthTiles(), CHUNK_WIDTH, CHUNK_REGION_WIDTH_CHUNKS, "chunk", 1024);

    LOG_INFO("  Height region count: {} - Patches Per Region - {}", mRegionFileClusters[e_cast(RegionType::Height)].getRegionCount(), mRegionFileClusters[e_cast(RegionType::Height)].getPatchesPerRegion());
    LOG_INFO("  Biome region count: {} - Patches Per Region - {}", mRegionFileClusters[e_cast(RegionType::Biome)].getRegionCount(), mRegionFileClusters[e_cast(RegionType::Biome)].getPatchesPerRegion());
    LOG_INFO("  Chunk region count: {} - Chunks Per Region - {}", mRegionFileClusters[e_cast(RegionType::Chunk)].getRegionCount(), mRegionFileClusters[e_cast(RegionType::Chunk)].getPatchesPerRegion());
    static_assert(e_count(RegionType) == 3);
}

WorldSaveContext::~WorldSaveContext() = default;

void WorldSaveContext::saveWorld(const fs::path& savePath) {
    assert(mCurrentSavePath.empty());
    mCurrentSavePath = savePath;

    WorldMarkupGrid& markupGrid = mWorld.getMarkupGrid();
    SimChunkTileGrid& tileGrid = mWorld.getSimTileGrid();

    PreciseTimer timer;
    saveWorldDesc();
    saveHeights();
    LOG_DEBUG("Heights took {} ms", timer.stop()); timer.start();
    saveBiomes();
    LOG_DEBUG("Biomes took {} ms", timer.stop()); timer.start();
    saveChunks();
    LOG_DEBUG("Chunks took {} ms", timer.stop()); timer.start();
    saveMarkupIfNotAlreadySaved();
    LOG_DEBUG("Markup took {} ms", timer.stop()); timer.start();


    // We can now finish saving
    notifyAllDataRegistered();
}

bool WorldSaveContext::loadWorld(const fs::path& loadPath) {
    PreciseTimer totalTimer;
    assert(mCurrentSavePath.empty());
    mCurrentSavePath = loadPath;

    WorldDesc desc = loadWorldDesc();
    // TODO: Allow other sizes
    assert(mWorld.getWidthTiles() == desc.worldWidth);

    PreciseTimer timer;
    loadHeights();
    LOG_DEBUG("Heights took {} ms", timer.stop()); timer.start();
    loadBiomes();
    LOG_DEBUG("Biomes took {} ms", timer.stop()); timer.start();
    loadChunks();
    LOG_DEBUG("Chunks took {} ms", timer.stop()); timer.start();
    loadMarkupSynchronous();
    LOG_DEBUG("Markup took {} ms", timer.stop()); timer.start();

    // Let saves finish
    while (mRunningLoadThreads) {
        Sleep(16);
    }

    LOG_DEBUG("Waited for {} ms", timer.stop());
    LOG_DEBUG("Load took {} ms", totalTimer.stop());
    mCurrentSavePath.clear();
    return true;
}

void WorldSaveContext::saveWorldDesc() {
    std::ofstream file(mCurrentSavePath / "world.desc", std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        panic("World save could not open world.desc for write");
    }
    BBuffer bbuffer;
    WorldDesc desc{ .worldWidth=mWorld.getWidthTiles() };
    const ui32 writtenBytes = bitsery::quickSerialization<BOutputAdapter>(bbuffer, WorldDesc{ mWorld.getWidthTiles() });
    file.write(reinterpret_cast<const char*>(bbuffer.data()), writtenBytes);
}

WorldSaveContext::WorldDesc WorldSaveContext::loadWorldDesc() {
    const fs::path descPath = mCurrentSavePath / "world.desc";
    if (!fs::exists(descPath)) {
        panic("World load missing world.desc");
    }
    BBuffer bbuffer(fs::file_size(descPath));

    std::ifstream file(descPath, std::ios::binary);
    if (!file.is_open()) {
        panic("World load could not open world.desc");
    }
    file.read(reinterpret_cast<char*>(bbuffer.data()), bbuffer.size());
    WorldDesc desc;
    auto state = bitsery::quickDeserialization(BInputAdapter{ bbuffer.data(), bbuffer.size()}, desc);
    assert(state.first == bitsery::ReaderError::NoError && state.second);
    return desc;
}

void WorldSaveContext::saveHeights() {
    assert(!mCurrentSavePath.empty());

    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    assert(heightGrid.mHeightData);
    const ui32 regionCount = getRegionCount(RegionType::Height);
    for (RegionID regionId = 0; regionId < regionCount; ++regionId) {
        ui32 pendingThisRegion = 0;
        forEachPatchInRegion(RegionType::Height, regionId, [this, &heightGrid, &pendingThisRegion](ui32 heightmapPatchID, RegionPatchID regionPatchId) {
            HeightmapPatch& patch = heightGrid.mHeightData[heightmapPatchID];
            if (patch.isSaveUpToDate.test_and_set() == false) {
                constexpr ui32 SSIZE = HEIGHTMAP_VERT_SIZE_PER_PATCH * sizeof(CompressedHeight);
                BBuffer buffer(SSIZE);
                const ui32 writtenBytes = bitsery::quickSerialization<BOutputAdapter>(buffer, patch);
                buffer.resize(writtenBytes);
                assert(writtenBytes == SSIZE);
                addRegionPatchCompressAndSaveTask(RegionType::Height, regionPatchId, std::move(buffer));
                ++pendingThisRegion;
            }
        });
        if (pendingThisRegion) {
            notifyRegionDataIncoming(RegionType::Height, regionId, pendingThisRegion);
        }
    }
}

void WorldSaveContext::loadHeights() {
    loadRegionsForType(RegionType::Height, [this](DeserializedRegionFileData&& fileData, RegionID regionId) {
        ++mRunningLoadThreads;
        Services::Threadpool::ref().addTask([this, regionId, fileData=std::move(fileData)]() mutable {
            forEachPatchInRegion(RegionType::Height, regionId, [this, regionId, &fileData](ui32 patchId, RegionPatchID regionPatchId) {
                IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();

                HeightmapPatch& patch = heightGrid.getPatchForGeneration(patchId);
                std::array<uint8_t, HEIGHTMAP_VERT_SIZE_PER_PATCH * sizeof(CompressedHeight)> dst;

                std::span<uint8_t> compressedBytes = fileData.getPatchBytes(regionPatchId.regionPatchIndex);
                size_t decompressedSize = decompressDataStatic(compressedBytes, dst.data(), sizeof(dst));
                assert(decompressedSize == sizeof(dst));
                bitsery::quickDeserialization(BInputAdapter{ dst.data(), sizeof(dst) }, patch);

                // Clean
                patch.isSaveUpToDate.test_and_set();
            });
            --mRunningLoadThreads;
        });
    });
}

void WorldSaveContext::saveBiomes() {
    BiomeGrid& biomeGrid = mWorld.getBiomeGrid();
    const ui32 regionCount = getRegionCount(RegionType::Biome);
    for (RegionID regionId = 0; regionId < regionCount; ++regionId) {
        ui32 pendingThisRegion = 0;
        forEachPatchInRegion(RegionType::Biome, regionId, [this, &biomeGrid, &pendingThisRegion](ui32 biomePatchId, RegionPatchID regionPatchId) {
            BiomePatch& patch = biomeGrid.mGrid[biomePatchId];
            if (biomeGrid.mPatchSavesUpToDate[biomePatchId].test_and_set() == false) {
                const ui32 SSIZE = patch.size() * sizeof(BiomeVertex) + 4;
                BBuffer buffer(SSIZE);
                bitsery::Serializer<BOutputAdapter> s{ BOutputAdapter{ buffer } };
                std::span<BiomeVertex> bSpan(patch.data(), patch.size());
                s.ext(bSpan, bitsery::ext::PodStructSpan{});
                s.adapter().flush();
                buffer.resize(s.adapter().writtenBytesCount());
                assert(buffer.size() == SSIZE);
                addRegionPatchCompressAndSaveTask(RegionType::Biome, regionPatchId, std::move(buffer));
                ++pendingThisRegion;
            }
        });
        if (pendingThisRegion) {
            notifyRegionDataIncoming(RegionType::Biome, regionId, pendingThisRegion);
        }
    }
}

void WorldSaveContext::loadBiomes() {
    loadRegionsForType(RegionType::Biome, [this](DeserializedRegionFileData&& fileData, RegionID regionId) {
        ++mRunningLoadThreads;
        Services::Threadpool::ref().addTask([this, regionId, fileData = std::move(fileData)]() mutable {
            forEachPatchInRegion(RegionType::Biome, regionId, [this, regionId, &fileData](ui32 patchId, RegionPatchID regionPatchId) {
                BiomeGrid& biomeGrid = mWorld.getBiomeGrid();
                BiomePatch& patch = biomeGrid.getPatchForLoad(patchId);

                const ui32 RSIZE = patch.size() * sizeof(BiomeVertex) + 4;


                std::span<uint8_t> compressedBytes = fileData.getPatchBytes(regionPatchId.regionPatchIndex);
                // TODO: Convert to static!
                BBuffer buffer = decompressDataStreamed(compressedBytes, RSIZE);
                if (buffer.size() > RSIZE) {
                    LOG_CRITICAL("Biome buffer reserve {} less than result {}", RSIZE, buffer.size());
                }
                bitsery::quickDeserialization(BInputAdapter{ buffer.data(), buffer.size()}, patch);

                // Clean
                biomeGrid.mPatchSavesUpToDate[patchId].test_and_set();
            });
            --mRunningLoadThreads;
        });
    });
}

void WorldSaveContext::saveChunks() {
    SimChunkTileGrid& simGrid = mWorld.getSimTileGrid();
    const ui32 regionCount = getRegionCount(RegionType::Chunk);
    for (RegionID regionId = 0; regionId < regionCount; ++regionId) {
        ui32 pendingThisRegion = 0;
        forEachPatchInRegion(RegionType::Chunk, regionId, [this, &simGrid, &pendingThisRegion](ui32 simChunkId, RegionPatchID regionPatchId) {
            SimChunkTileContainer& patch = simGrid.mChunkData[simChunkId];
            if (patch.isSaveUpToDate.test_and_set() == false) {
                // Oceans are implicit
                if (patch.getState() == SimChunkTileContainerState::Ocean) {
                    return;
                }
                BBuffer buffer;
                // TODO: Re-evaluate later
                //buffer.reserve(24000);
                const ui32 writtenBytes = bitsery::quickSerialization<BOutputAdapter>(buffer, patch);
                buffer.resize(writtenBytes);
                //if (writtenBytes > 24000) [[unlikely]] {
                //    LOG_CRITICAL(" B {}", writtenBytes);
                //}
                //buffer.shrink_to_fit();
                addRegionPatchCompressAndSaveTask(RegionType::Chunk, regionPatchId, std::move(buffer));
                ++pendingThisRegion;
            }
        });
        if (pendingThisRegion) {
            notifyRegionDataIncoming(RegionType::Chunk, regionId, pendingThisRegion);
        }
    }
}

void WorldSaveContext::loadChunks() {
    loadRegionsForType(RegionType::Chunk, [this](DeserializedRegionFileData&& fileData, RegionID regionId) {
        ++mRunningLoadThreads;
        Services::Threadpool::ref().addTask([this, regionId, fileData = std::move(fileData)]() mutable {
            forEachPatchInRegion(RegionType::Chunk, regionId, [this, regionId, &fileData](ui32 patchId, RegionPatchID regionPatchId) {
                SimChunkTileGrid& simGrid = mWorld.getSimTileGrid();
                SimChunkTileContainer& patch = simGrid.mChunkData[patchId];

                std::span<uint8_t> compressedBytes = fileData.getPatchBytes(regionPatchId.regionPatchIndex);
                // TODO: Evaluate reserve
                BBuffer buffer = decompressDataStreamed(compressedBytes, 2000);
                bitsery::quickDeserialization(BInputAdapter{ buffer.data(), buffer.size() }, patch);

                // Clean
                patch.isSaveUpToDate.test_and_set();
            });
            --mRunningLoadThreads;
        });
    });
}

// 500MB usually more than enough
constexpr size_t MARKUP_RESERVE = 500000000;

fs::path WorldSaveContext::getMarkupFilePath() const {
    return mCurrentSavePath / "markup" / "mkp.dat";
}

void WorldSaveContext::saveMarkupIfNotAlreadySaved() {
    const fs::path savePath = getMarkupFilePath();
    // Markup is const so only should be saved initially
    if (fs::exists(savePath)) {
        return;
    }

    if (!fs::exists(savePath.parent_path())) {
        if (!fs::create_directories(savePath.parent_path())) {
             panic("Failed to create {} directory. Insufficient permissions?", savePath.parent_path().string());
        }
    }

    WorldMarkupGrid& markupGrid = mWorld.getMarkupGrid();
    BBuffer buffer(MARKUP_RESERVE);
    const ui32 writtenBytes = bitsery::quickSerialization<BOutputAdapter>(buffer, markupGrid);
    buffer.resize(writtenBytes);

    ++mTotalIncomingData;
    Services::Threadpool::ref().addTask([this, buffer = std::move(buffer), savePath = std::move(savePath)]() {
        BBuffer compressed = compressData(buffer);
        GameSaveManager::get().addDiskIOTask([this, compressed = std::move(compressed), savePath = std::move(savePath)]() {
            
            // Dump markup to disk
            std::ofstream file(savePath, std::ios::binary);
            if (!file.is_open()) {
                panic("Failed to open markup file for writing: {}", savePath.string());
            }
            file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
            file.close();

            if (mAllIncomingDataRegistered && ++mTotalSavedData == mTotalIncomingData) {
                // Mark done
                endSave();
            }
        });
    });
}

void WorldSaveContext::loadMarkupSynchronous() {
    const fs::path savePath = getMarkupFilePath();
    // Markup is const so only should be saved initially
    if (!fs::exists(savePath)) {
        panic("Markup path {} not found during world load", savePath.string());
    }

    WorldMarkupGrid& markupGrid = mWorld.getMarkupGrid();
    BBuffer compressed(fs::file_size(savePath));
    std::ifstream file(savePath, std::ios::binary);
    if (!file.is_open()) {
        panic("Failed to open markup file for reading: {}", savePath.string());
    }
    file.read(reinterpret_cast<char*>(compressed.data()), compressed.size());
    file.close();

    const std::span<uint8_t> compressedBytes(compressed.data(), compressed.size());
    BBuffer buffer = decompressDataStreamed(compressedBytes, MARKUP_RESERVE);
    bitsery::quickDeserialization(BInputAdapter{ buffer.data(), buffer.size() }, markupGrid);
}

void WorldSaveContext::loadRegionsForType(RegionType type, std::function<void(DeserializedRegionFileData&&, RegionID)> func) {
    RegionFileCluster& fileCluster = mRegionFileClusters[e_cast(type)];
    const fs::path directoryPath = mCurrentSavePath / fileCluster.folderName;
    if (!fs::exists(directoryPath)) {
        panic("{} directory does not exist. Insufficient permissions?", directoryPath.string());
    }

    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    assert(heightGrid.mHeightData);
    const ui32 regionCount = getRegionCount(type);
    for (RegionID regionId = 0; regionId < regionCount; ++regionId) {
        fs::path regionPath = directoryPath / (std::to_string(regionId) + REGION_EXTENSION);
        if (!fs::exists(regionPath)) {
            panic("Could not find world heights {}", regionPath.string());
        }
        std::fstream file(regionPath, std::ios::binary | std::ios::in);
        if (!file.is_open()) {
            panic("Failed to open region file for reading: {}", regionPath.string());
        }
        DeserializedRegionFileData fileData = readRegionFile(file, fs::file_size(regionPath), type);
        func(std::move(fileData), regionId);
    }
}

void WorldSaveContext::notifyAllDataRegistered() {
    assert(!mAllIncomingDataRegistered);
    mAllIncomingDataRegistered = true;
    // Pretty unlikely but possible for small saves
    if (mTotalSavedData == mTotalIncomingData) {
        endSave();
    }
}

ui32 WorldSaveContext::getRegionCount(RegionType type) const {
    return mRegionFileClusters[e_cast(type)].getRegionCount();
}

void WorldSaveContext::forEachPatchInAllRegions(RegionType type, std::function<void(ui32 patchId, RegionPatchID regionPatchId)> callback) {
    RegionFileCluster& cluster = mRegionFileClusters[e_cast(type)];
    const ui32 regionCount = cluster.getRegionCount();
    for (RegionID i = 0; i < regionCount; ++i) {
        forEachPatchInRegion(type, i, callback);
    }
}

// TODO: Evaluate if templated functor is better https://stackoverflow.com/questions/18365532/should-i-pass-an-stdfunction-by-const-reference
void WorldSaveContext::forEachPatchInRegion(RegionType type, RegionID regionId, std::function<void(ui32 patchId, RegionPatchID regionPatchId)> callback) {
    RegionFileCluster& cluster = mRegionFileClusters[e_cast(type)];
    const ui32 worldWidthRegions = cluster.getWorldWidthRegions();
    const ui32 regionWidthPatches = cluster.getRegionWidthPatches();
    const ui32 worldWidthPatches = worldWidthRegions * regionWidthPatches;
    const ui32 regionXOff = (regionId / worldWidthRegions) * regionWidthPatches;
    const ui32 regionYOff = (regionId % worldWidthRegions) * regionWidthPatches;
    for (ui32 y = 0; y < regionWidthPatches; ++y) {
        const ui32 patchY = regionYOff + y;
        for (ui32 x = 0; x < regionWidthPatches; ++x) {
            const ui32 patchId = patchY * worldWidthPatches + regionXOff + x;
            callback(patchId, RegionPatchID{ regionId, y * regionWidthPatches + x });
        }
    };
    static_assert(e_count(RegionType) == 3);
}

void WorldSaveContext::notifyRegionDataIncoming(RegionType type, RegionID regionId, ui32 count) {
    RegionFileClusterWriteContext& writeContext = mWriteContexts[e_cast(type)];
    mTotalIncomingData += count;
    bool shouldDispatchIO = false;
    {
        RegionPendingWriteData* writeData;

        std::lock_guard lock(writeContext.mutex);
        auto&& it = writeContext.pendingWriteData.find(regionId);
        if (it != writeContext.pendingWriteData.end()) {
            writeData = &it->second;
            // Check if we already have all regions ready
            if (writeData->pendingWrites.size() == count) {
                shouldDispatchIO = true;
            }
        }
        else {
            writeData = &writeContext.pendingWriteData.emplace(regionId, RegionPendingWriteData{}).first->second;
            writeData->pendingWrites.reserve(mRegionFileClusters[e_cast(type)].getPatchesPerRegion());
        }
        assert(writeData->incomingWrites == 0);
        writeData->incomingWrites = count;
        
    }
    if (shouldDispatchIO) {
        dispatchRegionSaveTask(type, regionId);
    }
}

void WorldSaveContext::onRegionPatchDataReady(RegionType type, RegionPatchID patchId, BBuffer&& bbuffer) {
    RegionFileClusterWriteContext& writeContext = mWriteContexts[e_cast(type)];
    bool shouldDispatchIO = false;
    {
        RegionPendingWriteData* writeData;

        std::lock_guard lock(writeContext.mutex);
        auto&& it = writeContext.pendingWriteData.find(patchId.regionId);
        if (it != writeContext.pendingWriteData.end()) {
            writeData = &it->second;
            // Check if we already have all regions ready
            if (writeData->pendingWrites.size() + 1 == writeData->incomingWrites) {
                shouldDispatchIO = true;
            }
        }
        else {
            writeData = &writeContext.pendingWriteData.emplace(patchId.regionId, RegionPendingWriteData{}).first->second;
            writeData->pendingWrites.reserve(mRegionFileClusters[e_cast(type)].getPatchesPerRegion());
        }

        writeData->pendingWrites.emplace(patchId.regionPatchIndex, std::move(bbuffer));
    }
    if (shouldDispatchIO) {
        dispatchRegionSaveTask(type, patchId.regionId);
    }
}

void WorldSaveContext::dispatchRegionSaveTask(RegionType type, RegionID regionId) {
    // TODO: This could just be DiskIOThread interface instead of a backreference to GameSaveManager
    GameSaveManager::get().addDiskIOTask([this, type, regionId = regionId]() {
        updateRegionFile(type, regionId);

        RegionFileClusterWriteContext& writeContext = mWriteContexts[e_cast(type)];
        RegionPendingWriteData& writeData = writeContext.pendingWriteData[regionId];
        mTotalSavedData += writeData.pendingWrites.size();
        //LOG_INFO("Save {} {} {} {}", (int)type, mTotalSavedData, mTotalIncomingData, writeData.pendingWrites.size());
        if (mAllIncomingDataRegistered && mTotalSavedData == mTotalIncomingData) {
            // Mark done
            endSave();
        }
    });
}

void WorldSaveContext::addRegionPatchCompressAndSaveTask(RegionType type, RegionPatchID regionPatchId, BBuffer&& bbuffer) {
    Services::Threadpool::ref().addTask([this, buffer = std::move(bbuffer), type, regionPatchId]() mutable {
        BBuffer compressed;
        if (buffer.size() > 0) {
            compressed = compressData(buffer);
            BBuffer().swap(buffer);
        }
        onRegionPatchDataReady(type, regionPatchId, std::move(compressed));
    });
}

BBuffer WorldSaveContext::compressData(const BBuffer& bbuffer) {
    BBuffer compressed(ZSTD_compressBound(bbuffer.size()));
    // TODO: Dictionary compression for better speed and ratio
    size_t const cSize = ZSTD_compress(compressed.data(), compressed.size(), bbuffer.data(), bbuffer.size(), 1);
    if (ZSTD_isError(cSize)) {
        panic("ZSTD_compress failed");
    }
    else {
        compressed.resize(cSize);
    }
    // Improves speed by preventing too much memory from being held at once
    compressed.shrink_to_fit();
    return compressed;
}

size_t WorldSaveContext::decompressDataStatic(const std::span<uint8_t> compressed, uint8_t* dst, size_t dstSizeBytes) {
    size_t resultCount = ZSTD_decompress(dst, dstSizeBytes, compressed.data(), compressed.size());
    if (ZSTD_isError(resultCount)) {
        panic("ZSTD_decompress failed");
    }
    return resultCount;
}

BBuffer WorldSaveContext::decompressDataStreamed(const std::span<uint8_t> compressed, size_t reserveCount) {
    // Streaming decompression
    // https://raw.githack.com/facebook/zstd/release/doc/zstd_manual.html#Chapter9
    ZSTD_DStream* zds = ZSTD_createDStream();
    size_t recommendedSize = ZSTD_DStreamOutSize();

    ZSTD_initDStream(zds);
    
    BBuffer decompressed;
    decompressed.reserve(reserveCount);

    // TODO: This is pretty big
    BBuffer streamingBuffer(recommendedSize);
    ZSTD_inBuffer input;
    input.pos = 0;
    input.size = compressed.size();
    input.src = compressed.data();
    size_t lastRet = 0;
    while (input.pos < input.size) {
        ZSTD_outBuffer output = { streamingBuffer.data(), streamingBuffer.size(), 0 };

        size_t const ret = ZSTD_decompressStream(zds, &output, &input);
        if (ZSTD_isError(ret)) [[unlikely]] {
            LOG_CRITICAL("DECOMPRESSION ERROR: {} {}", ZSTD_getErrorName(ret), "(TODO: Handle this better)");
            return BBuffer();
        }

        decompressed.insert(decompressed.end(), streamingBuffer.begin(), streamingBuffer.begin() + output.pos);
        lastRet = ret;
    }

    if (lastRet != 0) {
        /* The last return value from ZSTD_decompressStream did not end on a
         * frame, but we reached the end of the file! We assume this is an
         * error, and the input was truncated.
         */
        LOG_CRITICAL("DECOMPRESSION EOF before end of stream: {}", lastRet);
        return BBuffer();
    }

    decompressed.shrink_to_fit();

    ZSTD_freeDStream(zds);
    return decompressed;
}

DeserializedRegionFileData WorldSaveContext::readRegionFile(std::fstream& file, ui32 fileSize, RegionType type) {
    if (!file.is_open() || !fileSize) {
        return DeserializedRegionFileData();
    }
    RegionFileCluster& fileCluster = mRegionFileClusters[e_cast(type)];

    DeserializedRegionFileData data;
    data.fileBytes.resize(fileSize);
    file.read(reinterpret_cast<char*>(data.fileBytes.data()), fileSize);
    auto state = bitsery::quickDeserialization(BInputAdapter{ data.fileBytes.data(), fileCluster.getHeaderSerializeSizeBytes()}, data.header);
    assert(state.first == bitsery::ReaderError::NoError && state.second);
    return data;
}

i32 getDesiredPages(size_t bufferSize, ui32 pageSize) {
    return (bufferSize == 0) ? 0 : (i32)((bufferSize + pageSize - 1) / pageSize);
}

void WorldSaveContext::updateRegionFile(RegionType type, RegionID regionId) {
    RegionFileCluster& fileCluster = mRegionFileClusters[e_cast(type)];
    RegionFileClusterWriteContext& writeContext = mWriteContexts[e_cast(type)];
    RegionPendingWriteData& writeData = writeContext.pendingWriteData[regionId];
    RegionFileHeader& header = fileCluster.mRegionHeaders[regionId];
    const ui32 pageSize = fileCluster.getPageSize();
    const ui32 headerSizeBytes = fileCluster.getHeaderSerializeSizeBytes();
    //TODO: Detect if file must be resized, save file data to disk
    //TODO: Only append starting at the first dirty patch instead of rewriting the whole file
    const fs::path directoryPath = mCurrentSavePath / fileCluster.folderName;
    if (!fs::exists(directoryPath)) {
        if (!fs::create_directories(directoryPath)) {
            panic("Failed to create {} directory. Insufficient permissions?", directoryPath.string());
        }
    }

    fs::path regionPath = directoryPath / (std::to_string(regionId) + REGION_EXTENSION);

    RegionPatchIndex firstDirtyAllocation = UINT32_MAX;
    ui32 totalDesiredPages = 0;

    // Detect first dirty allocation position
    for (auto& [patchId, buffer] : writeData.pendingWrites) {
        const i32 desiredPages = getDesiredPages(buffer.size(), pageSize);
        totalDesiredPages += desiredPages;
        RegionPatchDesc& desc = header.patches[patchId];
        // We only shrink allocation if we are a whole page smaller to prevent ping ponging
        if (firstDirtyAllocation == UINT32_MAX && (desiredPages > desc.mAllocatedPages || desiredPages < desc.mAllocatedPages - 1)) [[unlikely]] {
            firstDirtyAllocation = patchId;
        }
    }

    // Remove files that are empty
    if (totalDesiredPages == 0) {
        fs::remove(regionPath);
        return;
    }

    const ui32 totalDesiredFileSize = headerSizeBytes + totalDesiredPages * pageSize;

    std::fstream file;
    if (firstDirtyAllocation == UINT32_MAX) {
        assert(fs::exists(regionPath));
        file.open(regionPath, std::ios::binary | std::ios::in | std::ios::out);
        // Case 1: Simple and fast - no reallocations are needed, we can just serialize dirty parts and update sizes
        // this case ideally is usually what happens thanks to PAGE_SIZE being large enough
        for (auto& [patchId, buffer] : writeData.pendingWrites) {
            RegionPatchDesc& desc = header.patches[patchId];
            desc.mAllocatedBytes = buffer.size();
            file.seekp(desc.mStartByte);
            file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
        }
    }
    else {
        // Case 2: We need to reallocate parts of the file
        // Resize the file if needed and open
        DeserializedRegionFileData previousFileData;
        const bool fileExisted = fs::exists(regionPath);
        if (!fileExisted || totalDesiredFileSize >= fs::file_size(regionPath)) {
            if (fileExisted) {
                file.open(regionPath, std::ios::binary | std::ios::in | std::ios::out);
                previousFileData = readRegionFile(file, fs::file_size(regionPath), type);
            }
            else {
                file.open(regionPath, std::ios::binary | std::ios::out);
            }
            if (!file.is_open()) {
                panic("Failed to open region file for writing: {}", regionPath.string());
            }
            file.seekp(totalDesiredFileSize - 1);
            file.write("", 1);
            file.flush();
        }
        else {
            file.open(regionPath, std::ios::binary | std::ios::in | std::ios::out);
            if (!file.is_open()) {
                panic("Failed to open region file for writing: {}", regionPath.string());
            }
            previousFileData = readRegionFile(file, fs::file_size(regionPath), type);
            if (totalDesiredFileSize != fs::file_size(regionPath)) {
                fs::resize_file(regionPath, totalDesiredFileSize);
            }
        }

        // Helper for writing previous unmodified patch data to disk
        auto writePreviousPatchData = [&file, &previousFileData, pageSize](RegionFileHeader& header, ui32& startByte, ui32 startPatchId, ui32 termPatchId) {
            for (ui32 prev = startPatchId; prev < termPatchId; ++prev) {
                RegionPatchDesc& prevDesc = header.patches[prev];
                prevDesc = previousFileData.header.patches[prev];
                prevDesc.mStartByte = startByte;
                if (prevDesc.mAllocatedPages) {
                    std::span<uint8_t> prevBytes = previousFileData.getPatchBytes(prev);
                    file.seekp(startByte);
                    file.write(reinterpret_cast<char*>(prevBytes.data()), prevBytes.size());
                    startByte += prevDesc.mAllocatedPages * pageSize;
                }
            }
        };

        ui32 unmodifiedStartPatchId = 0;
        ui32 startByte = headerSizeBytes;
        for (auto& [patchId, buffer] : writeData.pendingWrites) {
            RegionPatchDesc& desc = header.patches[patchId];
            desc.mAllocatedBytes = buffer.size();
            desc.mStartByte = startByte;
            if (patchId < firstDirtyAllocation) {
                // Just write directly if we haven't resized yet
                file.seekp(startByte);
                file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
            }
            else {
                // Write unmodified patches from previous file
                if (previousFileData.isValid()) {
                    writePreviousPatchData(header, startByte, unmodifiedStartPatchId, patchId);
                }
                // Write dirty patch
                desc.mAllocatedPages = getDesiredPages(buffer.size(), pageSize);
                if (desc.mAllocatedPages) {
                    file.seekp(startByte);
                    file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());

                    startByte += desc.mAllocatedPages * pageSize;
                }
                else {
                    assert(desc.mAllocatedBytes == 0);
                }
            }
            unmodifiedStartPatchId = patchId + 1;
        }
        // Write trailing patches
        if (previousFileData.isValid()) {
            writePreviousPatchData(header, startByte, unmodifiedStartPatchId, header.patches.size());
        }
    }
    
    // Update header last
    // Serialize the entire header every time
    // TODO: Is seeking and updating dirty more efficient?
    BBuffer headerBuffer;
    headerBuffer.resize(headerSizeBytes);
    const ui32 writtenBytes = bitsery::quickSerialization<BOutputAdapter>(headerBuffer, header);
    assert(writtenBytes == headerSizeBytes);
    file.seekp(0);
    file.write(reinterpret_cast<char*>(headerBuffer.data()), headerSizeBytes);
}

void WorldSaveContext::endSave() {
    // In case we end save on main thread and in worker thread (very rare race condition)
    std::lock_guard lock(mEndSaveMutex);

    if (mCurrentSavePath.empty()) {
        return;
    }

    WorldSaveEvent evnt;
    evnt.savePath = mCurrentSavePath;

    std::swap(mPrevSavePath, mCurrentSavePath);
    mCurrentSavePath.clear();
    mAllIncomingDataRegistered = false;

    for (auto&& context : mWriteContexts) {
        std::lock_guard lock(context.mutex);
        context.pendingWriteData.clear();
    }
    dispatchSaveEnd(evnt);
}

