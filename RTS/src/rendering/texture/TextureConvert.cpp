#include "stdafx.h"
#include "TextureConvert.h"

#include <extern/bc7enc/rdo_bc_encoder.h>

bool supportsBC7 = false;
bc7enc_compress_block_params pack_params;

constexpr int uber_level = 0; // Goes up to 4  // TODO: 4 FOR SHIPPING BUILDS TO PACKAGE DDS?
constexpr int max_partitions_to_scan = BC7ENC_MAX_PARTITIONS;
constexpr bool perceptual = true; // TODO: ENABLE FOR SHIPPING BUILDS TO PACKAGE DDS?
constexpr bool y_flip = false;
constexpr uint32_t bc45_channel0 = 0;
constexpr uint32_t bc45_channel1 = 1;


constexpr rgbcx::bc1_approx_mode bc1_mode = rgbcx::bc1_approx_mode::cBC1Ideal; // TODO: nvidia vs amd instead of cBC1Ideal?
constexpr bool use_bc1_3color_mode = true;
constexpr bool use_bc1_3color_mode_for_black = false;
constexpr int bc1_quality_level = (int)rgbcx::MAX_LEVEL;

constexpr uint32_t pixel_format_bpp = 8;
constexpr bool force_dx10_dds = false;

template <size_t Channels>
void generateMipmaps(gli::texture2d& texture) {
    static_assert(Channels >= 1 && Channels <= 4, "Invalid number of channels");
    using ColorType = glm::vec<Channels, uint8_t>;
    using FColorType = glm::vec<Channels, f32>;

    for (size_t level = 1; level < texture.levels(); ++level) {
        auto prevLevelExtent = texture.extent(level - 1);
        auto currLevelExtent = texture.extent(level);
        float scaleX = prevLevelExtent.x / static_cast<float>(currLevelExtent.x);
        float scaleY = prevLevelExtent.y / static_cast<float>(currLevelExtent.y);

        for (size_t y = 0; y < currLevelExtent.y; ++y) {
            float srcY = (y + 0.5f) * scaleY;
            int srcY1 = static_cast<int>(srcY);
            int srcY2 = srcY1 + 1;
            srcY2 = glm::min(srcY2, static_cast<int>(prevLevelExtent.y - 1));
            float fracY = srcY - srcY1;

            for (size_t x = 0; x < currLevelExtent.x; ++x) {
                float srcX = (x + 0.5f) * scaleX;
                int srcX1 = static_cast<int>(srcX);
                int srcX2 = srcX1 + 1;
                srcX2 = glm::min(srcX2, static_cast<int>(prevLevelExtent.x - 1));
                float fracX = srcX - srcX1;

                FColorType p00(texture.load<ColorType>(gli::texture2d::extent_type(srcX1, srcY1), level - 1));
                FColorType p10(texture.load<ColorType>(gli::texture2d::extent_type(srcX2, srcY1), level - 1));
                FColorType p01(texture.load<ColorType>(gli::texture2d::extent_type(srcX1, srcY2), level - 1));
                FColorType p11(texture.load<ColorType>(gli::texture2d::extent_type(srcX2, srcY2), level - 1));

                float one_minus_fracX = 1.0f - fracX;
                float one_minus_fracY = 1.0f - fracY;

                ColorType color = ColorType((p00 * one_minus_fracX + p10 * fracX) * one_minus_fracY +
                    (p01 * one_minus_fracX + p11 * fracX) * fracY);

                texture.store(gli::texture2d::extent_type(x, y), level, color);
            }
        }
    }
}


void TextureConvert::initConverters() {
    static bool needsInit = true;
    if (needsInit) {
        LOG_INFO("Initializing texture converters");
        // DDS init
        supportsBC7 = false;// glewIsSupported("GL_ARB_texture_compression_bptc");

        if (supportsBC7) {
            bc7enc_compress_block_params_init(&pack_params);
            if (!perceptual)
                bc7enc_compress_block_params_init_linear_weights(&pack_params);
            pack_params.m_max_partitions = max_partitions_to_scan;
            pack_params.m_uber_level = std::min(BC7ENC_MAX_UBER_LEVEL, uber_level);
            bc7enc_compress_block_init();
            printf("  BCe7 Enabled: Max mode 1 partitions: %u, uber level: %u, perceptual: %u\n", pack_params.m_max_partitions, pack_params.m_uber_level, perceptual);
        }

        printf(" BC1 Level: %u, use 3-color mode: %u, use 3-color mode for black: %u, bc1_mode: %u\n",
            bc1_quality_level, use_bc1_3color_mode, use_bc1_3color_mode_for_black, (int)bc1_mode);

        rgbcx::init(bc1_mode);

        needsInit = false;
    }
}

gli::texture2d TextureConvert::convertToR8(const gli::texture2d& inputTexture) {
    constexpr int MAX_LEVEL = 1;
    const int numChannels = gli::component_count(inputTexture.format());
    const int byteDepth = gli::block_size(inputTexture.format()) / numChannels;
    if (byteDepth != 1) {
        LOG_CRITICAL("Unimplemented byte depth {} in convertToR8", byteDepth);
        throw std::exception("Invalid block size in convertToR8");
    }
    if (numChannels == 1) {
        LOG_CRITICAL("Tried to convert a texture that was already r8");
        throw std::exception("Tried to convert a texture that was already r8");
    }
    gli::texture2d newTexture(gli::FORMAT_R8_UNORM_PACK8, inputTexture.extent(), MAX_LEVEL);
    const ui8* inputData = inputTexture.data<ui8>();
    ui8* outputData = newTexture.data<ui8>();
    for (std::size_t i = 0; i < newTexture.size(0); ++i) {
        const ui8* pixelData = inputData + i * numChannels;
        outputData[i] = pixelData[0];  // Grab R channel
    }
    return newTexture;
}

struct block8
{
    uint64_t m_vals[1];
};
typedef std::vector<block8> block8_vec;

struct block16
{
    uint64_t m_vals[2];
};
typedef std::vector<block16> block16_vec;

struct color_quad_u8
{
    uint8_t m_c[4];

    inline color_quad_u8(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        set(r, g, b, a);
    }

    inline color_quad_u8(uint8_t y = 0, uint8_t a = 255)
    {
        set(y, a);
    }

    inline color_quad_u8& set(uint8_t y, uint8_t a = 255)
    {
        m_c[0] = y;
        m_c[1] = y;
        m_c[2] = y;
        m_c[3] = a;
        return *this;
    }

    inline color_quad_u8& set(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        m_c[0] = r;
        m_c[1] = g;
        m_c[2] = b;
        m_c[3] = a;
        return *this;
    }

    inline uint8_t& operator[] (uint32_t i) { assert(i < 4);  return m_c[i]; }
    inline uint8_t operator[] (uint32_t i) const { assert(i < 4); return m_c[i]; }

    inline int get_luma() const { return (13938U * m_c[0] + 46869U * m_c[1] + 4729U * m_c[2] + 32768U) >> 16U; } // REC709 weightings
};
typedef std::vector<color_quad_u8> color_quad_u8_vec;

inline void get_block(const gli::texture2d& inputTexture, uint32_t blockx, uint32_t blocky, color_quad_u8* outPixels, int level)
{
    const int num_channels = gli::component_count(inputTexture.format());
    const int inputRowStride = inputTexture.extent(level).x * num_channels;
    const ui8* inputData = inputTexture.data<ui8>(0, 0, level) + blockx * num_channels * 4 + blocky * inputRowStride * 4;
    for (uint32_t y = 0; y < 4; y++) {
        for (uint32_t x = 0; x < 4; x++) {
            color_quad_u8& pixel = outPixels[y * 4 + x];
            const ui8* inputPixelData = inputData + y * inputRowStride + x * num_channels;
            pixel.set(inputPixelData[0], 0, 0, 255); // Default value: Black color, fully opaque

            if (num_channels >= 2) {
                pixel[1] = inputPixelData[1]; // G
                if (num_channels >= 3) {
                    pixel[2] = inputPixelData[2]; // B
                    if (num_channels >= 4) {
                        pixel[3] = inputPixelData[3]; // A
                    }
                }
            }
        }
    }
}

// Based on bc7enc/test.cpp
gli::texture2d TextureConvert::convertToDDS(gli::texture2d& inputTexture, bool shouldGenMipmaps) {

    clock_t start_t = clock();

    const int num_channels = gli::component_count(inputTexture.format());

    PreciseTimer timer;
    gli::texture2d* srcTexture = &inputTexture;
    if (shouldGenMipmaps) {
        switch (num_channels) {
            case 1:
                generateMipmaps<1>(inputTexture);
                break;
            case 2:
                generateMipmaps<2>(inputTexture);
                break;
            case 3:
                generateMipmaps<3>(inputTexture);
                break;
            case 4:
                generateMipmaps<4>(inputTexture);
                break;
            default:
                assert(false);
        }
        LOG_TRACE("Generated mipmaps in {} ms", timer.stop());
        timer.start();

    }

    // NOTE: BC7 is VERY slow, and is larger in memory, but is much higher quality
    // Consider selecting specific textures to be compressed via BC7 or dont use it at all
    const bool supportsBC7 = false;// glewIsSupported("GL_ARB_texture_compression_bptc");

    const int block_size = gli::block_size(srcTexture->format());
    const int byte_depth = block_size / num_channels;
    assert(byte_depth == 1 && "Need to handle >= 16 bit components in convertToDDS");

    assert(srcTexture->extent().x % 4 == 0);
    assert(srcTexture->extent().y % 4 == 0);


    const uint32_t top_blocks_x = srcTexture->extent().x / 4;
    const uint32_t top_blocks_y = srcTexture->extent().y / 4;
    const uint32_t top_totalBlocks = top_blocks_x * top_blocks_y;

    LOG_TRACE("Compressing to DDS...");

    gli::texture2d texture;

    DXGI_FORMAT dxgi_format = DXGI_FORMAT_BC3_UNORM;
    switch (inputTexture.format()) {
        case gli::FORMAT_R8_UNORM_PACK8:
            dxgi_format = DXGI_FORMAT_BC4_UNORM;
            texture = gli::texture2d(gli::FORMAT_R_ATI1N_UNORM_BLOCK8, gli::texture2d::extent_type(top_blocks_x * 4, top_blocks_y * 4));
            break;
        case gli::FORMAT_RG8_UNORM_PACK8:
            dxgi_format = DXGI_FORMAT_BC5_UNORM;
            texture = gli::texture2d(gli::FORMAT_RG_ATI2N_UNORM_BLOCK16, gli::texture2d::extent_type(top_blocks_x * 4, top_blocks_y * 4));
            break;
        case gli::FORMAT_RGB8_UNORM_PACK8:
            dxgi_format = DXGI_FORMAT_BC1_UNORM;
            texture = gli::texture2d(gli::FORMAT_RGB_DXT1_UNORM_BLOCK8, gli::texture2d::extent_type(top_blocks_x * 4, top_blocks_y * 4));
            break;
        case gli::FORMAT_RGBA8_UNORM_PACK8:
            if (supportsBC7) {
                dxgi_format = DXGI_FORMAT_BC7_UNORM;
                texture = gli::texture2d(gli::FORMAT_RGBA_BP_UNORM_BLOCK16, gli::texture2d::extent_type(top_blocks_x * 4, top_blocks_y * 4));
            }
            else {
                dxgi_format = DXGI_FORMAT_BC3_UNORM;
                texture = gli::texture2d(gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16, gli::texture2d::extent_type(top_blocks_x * 4, top_blocks_y * 4));

                dxgi_format = supportsBC7 ? DXGI_FORMAT_BC7_UNORM : DXGI_FORMAT_BC3_UNORM;
                break;
            }
        default:
            assert(false && "Unsupported format in convertToDDS");
            throw std::exception("Unsupported format in convertToDDS");
    }

    utils::image_u8 source_image(inputTexture.extent().x, inputTexture.extent().y);
    for (int y = 0; y < inputTexture.extent().y; y++) {
        for (int x = 0; x < inputTexture.extent().x; x++) {
            const int num_channels = gli::component_count(inputTexture.format());
            const int inputRowStride = inputTexture.extent().x * num_channels;
            const ui8* pixelData = inputTexture.data<ui8>(0, 0, 0) + x * num_channels + y * inputRowStride;
            source_image(x, y)[0] = pixelData[0];
            if (num_channels >= 2) {
                source_image(x, y)[1] = pixelData[1];
                if (num_channels >= 3) {
                    source_image(x, y)[2] = pixelData[2];
                    if (num_channels >= 4) {
                        source_image(x, y)[3] = pixelData[3];
                    }
                }
            }
        }
    }
    //rdo_bc::rdo_bc_encoder encoder;
    //rdo_bc::rdo_bc_params rp;
    //rp.m_dxgi_format = dxgi_format;

    //if (!encoder.init(source_image, rp))
    //{
    //    fprintf(stderr, "rdo_bc_encoder::init() failed!\n");
    //    assert(false);
    //}

    //if (rp.m_status_output)
    //{
    //    if (encoder.get_has_alpha())
    //        printf("Source image has an alpha channel.\n");
    //    else
    //        printf("Source image is opaque.\n");
    //}

    //if (!encoder.encode())
    //{
    //    fprintf(stderr, "rdo_bc_encoder::encode() failed!\n");
    //    assert(false);
    //}

    //const uint8_t* pBlock = (const uint8_t*)encoder.get_blocks();
    //LOG_INFO("{} {}", encoder.get_total_blocks_size_in_bytes(), texture.size());
    //assert(encoder.get_total_blocks_size_in_bytes() <= texture.size() && "Texture size mismatch");
    //memcpy(texture.data(0,0,0), pBlock, encoder.get_total_blocks_size_in_bytes());

    //return texture;

    // For each mip level
    for (uint32_t mip = 0; mip < srcTexture->levels(); ++mip) {
        gli::extent2d mipExtent = srcTexture->extent(mip);
        if ((mipExtent.x % 4) != 0 && mipExtent.x > 4) {
            panic("Texture mip level x was not divisible by 4. DDS compression failed. Ensure all textures are PO2");
        }
        if ((mipExtent.y % 4) != 0 && mipExtent.y > 4) {
            panic("Texture mip level x was not divisible by 4. DDS compression failed. Ensure all textures are PO2");
        }
        // But we will need to guarentee that above when allocating the texture
        const uint32_t blocks_x = (mipExtent.x + 3) / 4;
        const uint32_t blocks_y = (mipExtent.y + 3) / 4;
        const uint32_t totalBlocks = blocks_x * blocks_y;

        // Allocate memory and init
        block16_vec packed_image16;
        block8_vec packed_image8;
        switch (dxgi_format) {
            case DXGI_FORMAT_BC1_UNORM: // DXT1 RGB
            case DXGI_FORMAT_BC4_UNORM: // Greyscale
                packed_image8.resize(totalBlocks);
                break;
            case DXGI_FORMAT_BC3_UNORM: // DXT5 RGBA
            case DXGI_FORMAT_BC5_UNORM: // DXT5 RG
            case DXGI_FORMAT_BC7_UNORM: // RGBA High fidelity (not always supported)
                packed_image16.resize(totalBlocks);
                break;
            default:
                assert(0);
                break;
        }

        for (uint32_t by = 0; by < blocks_y; by++)
        {
            for (uint32_t bx = 0; bx < blocks_x; bx++)
            {
                color_quad_u8 pixels[16];

                get_block(*srcTexture, bx, by, pixels, mip);

                switch (dxgi_format)
                {
                    case DXGI_FORMAT_BC1_UNORM: // DXT1 RGB
                    {
                        block8* pBlock = &packed_image8[bx + by * blocks_x];
                        rgbcx::encode_bc1(bc1_quality_level, pBlock, &pixels[0].m_c[0], use_bc1_3color_mode, use_bc1_3color_mode_for_black);
                        break;
                    }
                    case DXGI_FORMAT_BC3_UNORM: // DXT5 RGBA
                    {
                        block16* pBlock = &packed_image16[bx + by * blocks_x];
                        rgbcx::encode_bc3(bc1_quality_level, pBlock, &pixels[0].m_c[0]);
                        break;
                    }
                    case DXGI_FORMAT_BC4_UNORM: // Greyscale
                    {
                        block8* pBlock = &packed_image8[bx + by * blocks_x];
                        rgbcx::encode_bc4(pBlock, &pixels[0].m_c[bc45_channel0], 4);
                        break;
                    }
                    case DXGI_FORMAT_BC5_UNORM: // RG
                    {
                        block16* pBlock = &packed_image16[bx + by * blocks_x];
                        rgbcx::encode_bc5(pBlock, &pixels[0].m_c[0], bc45_channel0, bc45_channel1, 4);
                        break;
                    }
                    case DXGI_FORMAT_BC7_UNORM: // RGBA High fidelity (not always supported)
                    {
                        block16* pBlock = &packed_image16[bx + by * blocks_x];
                        bc7enc_compress_block(pBlock, pixels, &pack_params);
                        break;
                    }
                    default:
                    {
                        assert(0);
                        break;
                    }
                }
            }
        }

        switch (dxgi_format)
        {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC4_UNORM:
                memcpy(texture.data(0, 0, mip), packed_image8.data(), packed_image8.size() * sizeof(block8));
                break;
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC5_UNORM:
            case DXGI_FORMAT_BC7_UNORM:
                memcpy(texture.data(0, 0, mip), packed_image16.data(), packed_image16.size() * sizeof(block16));
                break;
            default: {
                assert(0);
                break;
            }
        }
    }

    clock_t end_t = clock();
    LOG_TRACE("Total DDS time: {} secs", (double)(end_t - start_t) / CLOCKS_PER_SEC);

    return texture;
}
