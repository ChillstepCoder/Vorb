#include "stdafx.h"
#include "TextureConvert.h"

#include <extern/bc7enc/bc7enc.h>
#include <extern/bc7enc/dds_defs.h>
#include <extern/bc7enc/bc7decomp.h>

#define RGBCX_IMPLEMENTATION
#include <extern/bc7enc/rgbcx.h>

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
    for (std::size_t i = 0; i < newTexture.size(0); ++i) {
        const ui8* pixelData = inputTexture.data<ui8>() + i * numChannels;
        newTexture.data<ui8>()[i] = pixelData[0];  // Grab R channel
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

inline void get_block(const gli::texture2d& inputTexture, uint32_t blockx, uint32_t blocky, color_quad_u8* outPixels)
{
	const int num_channels = gli::component_count(inputTexture.format());
	const int inputRowStride = inputTexture.extent().x * num_channels;
	const ui8* inputData = inputTexture.data<ui8>() + blockx * num_channels * 4 + blocky * inputRowStride * 4;
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
gli::texture2d TextureConvert::convertToDDS(const gli::texture2d& inputTexture) {

	const bool supportsBC7 = glewIsSupported("GL_ARB_texture_compression_bptc");

	const int num_channels = gli::component_count(inputTexture.format());
	const int block_size = gli::block_size(inputTexture.format());
	const int byte_depth = block_size / num_channels;
	assert(byte_depth == 1 && "Need to handle >= 16 bit components in convertToDDS");

	assert(inputTexture.extent().x % 4 == 0);
	assert(inputTexture.extent().y % 4 == 0);

	LOG_INFO("Compressing to DDS...");

	int uber_level = 0; // Goes up to 4
	int max_partitions_to_scan = BC7ENC_MAX_PARTITIONS1;
	bool perceptual = true;
	bool y_flip = false;
	uint32_t bc45_channel0 = 0;
	uint32_t bc45_channel1 = 1;

	rgbcx::bc1_approx_mode bc1_mode = rgbcx::bc1_approx_mode::cBC1Ideal;
	bool use_bc1_3color_mode = true;
	bool use_bc1_3color_mode_for_black = false;
	int bc1_quality_level = 2;

	DXGI_FORMAT dxgi_format = DXGI_FORMAT_BC3_UNORM;
	switch (inputTexture.format()) {
		case gli::FORMAT_R8_UNORM_PACK8:
			dxgi_format = DXGI_FORMAT_BC4_UNORM;
			break;
		case gli::FORMAT_RGB8_UNORM_PACK8:
			dxgi_format = DXGI_FORMAT_BC1_UNORM;
			break;
		case gli::FORMAT_RGBA8_UNORM_PACK8:
			dxgi_format = supportsBC7 ? DXGI_FORMAT_BC7_UNORM : DXGI_FORMAT_BC3_UNORM;
			break;
		default:
			assert(false && "Unsupported format in convertToDDS");
			throw std::exception("Unsupported format in convertToDDS");
	}
	uint32_t pixel_format_bpp = 8;
	bool force_dx10_dds = false;

	const uint32_t blocks_x = inputTexture.extent().x / 4;
	const uint32_t blocks_y = inputTexture.extent().y / 4;
	const uint32_t totalBlocks = blocks_x * blocks_y;

	block16_vec packed_image16(totalBlocks);
	block8_vec packed_image8(totalBlocks);

	bc7enc_compress_block_params pack_params;
	bc7enc_compress_block_params_init(&pack_params);
	if (!perceptual)
		bc7enc_compress_block_params_init_linear_weights(&pack_params);
	pack_params.m_max_partitions_mode = max_partitions_to_scan;
	pack_params.m_uber_level = std::min(BC7ENC_MAX_UBER_LEVEL, uber_level);

	if (dxgi_format == DXGI_FORMAT_BC7_UNORM)
	{
		printf("  Max mode 1 partitions: %u, uber level: %u, perceptual: %u\n", pack_params.m_max_partitions_mode, pack_params.m_uber_level, perceptual);
	}
	else
	{
		printf("  Level: %u, use 3-color mode: %u, use 3-color mode for black: %u, bc1_mode: %u\n",
			bc1_quality_level, use_bc1_3color_mode, use_bc1_3color_mode_for_black, (int)bc1_mode);
	}

	bc7enc_compress_block_init();
	rgbcx::init(bc1_mode);

	clock_t start_t = clock();

	for (uint32_t by = 0; by < blocks_y; by++)
	{
		for (uint32_t bx = 0; bx < blocks_x; bx++)
		{
			color_quad_u8 pixels[16];

			get_block(inputTexture, bx, by, pixels);

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
					// CURRENTLY UNSUPPORTED
					//block16* pBlock = &packed_image16[bx + by * blocks_x];
					//rgbcx::encode_bc5(pBlock, &pixels[0].m_c[0], bc45_channel0, bc45_channel1, 4);
					assert(false);
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

	gli::texture2d texture;
	switch (dxgi_format)
	{
		case DXGI_FORMAT_BC1_UNORM:
			texture = gli::texture2d(gli::FORMAT_RGB_DXT1_UNORM_BLOCK8, gli::texture2d::extent_type(blocks_x * 4, blocks_y * 4), 1);
			memcpy(texture.data(), packed_image8.data(), packed_image8.size() * sizeof(block8));
			break;
		case DXGI_FORMAT_BC3_UNORM:
			texture = gli::texture2d(gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16, gli::texture2d::extent_type(blocks_x * 4, blocks_y * 4), 1);
			memcpy(texture.data(), packed_image16.data(), packed_image16.size() * sizeof(block16));
			break;
		case DXGI_FORMAT_BC4_UNORM:
			texture = gli::texture2d(gli::FORMAT_R_ATI1N_UNORM_BLOCK8, gli::texture2d::extent_type(blocks_x * 4, blocks_y * 4), 1);
			memcpy(texture.data(), packed_image8.data(), packed_image8.size() * sizeof(block8));
			break;
		case DXGI_FORMAT_BC7_UNORM:
			texture = gli::texture2d(gli::FORMAT_RGBA_BP_UNORM_BLOCK16, gli::texture2d::extent_type(blocks_x * 4, blocks_y * 4), 1);
			memcpy(texture.data(), packed_image16.data(), packed_image16.size() * sizeof(block16));
			break;
		default: {
			assert(0);
			break;
		}
	}

	clock_t end_t = clock();
	LOG_INFO("Total time: {} secs", (double)(end_t - start_t) / CLOCKS_PER_SEC);

	return texture;
}
