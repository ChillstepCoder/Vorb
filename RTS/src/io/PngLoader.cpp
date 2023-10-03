#include "stdafx.h"
#include "PngLoader.h"

#include <png.h>


bool isPowerOfTwo(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

constexpr int MAX_TEXTURE_DIMENSION = 4096;

gli::texture2d allocateTexture(const ui32& w, const ui32& h, int byteDepth, int channels) {
    // NOTE: We are using all UNORM formats because we are doing sRGB gamma correction in
    // the shaders.
    gli::extent2d dimensions{ w, h };
    size_t size = 0;
    if (byteDepth == 1) {
        switch (channels) {
            case 1:
                return gli::texture2d(gli::FORMAT_R8_UNORM_PACK8, dimensions);
            case 2:
                return gli::texture2d(gli::FORMAT_RG8_UNORM_PACK8, dimensions);
            case 3:
                return gli::texture2d(gli::FORMAT_RGB8_UNORM_PACK8, dimensions);
            case 4:
                return gli::texture2d(gli::FORMAT_RGBA8_UNORM_PACK8, dimensions);
            default:
                throw std::exception("Invalid channel count");
        }
    }
    else if (byteDepth == 2) {
        switch (channels) {
            case 1:
                return gli::texture2d(gli::FORMAT_R16_UNORM_PACK16, dimensions);
            case 2:
                return gli::texture2d(gli::FORMAT_RG16_UNORM_PACK16, dimensions);
            case 3:
                return gli::texture2d(gli::FORMAT_RGB16_UNORM_PACK16, dimensions);
            case 4:
                return gli::texture2d(gli::FORMAT_RGBA16_UNORM_PACK16, dimensions);
            default:
                throw std::exception("Invalid channel count");
        }
    }
    LOG_CRITICAL("Missing byte depth {} in allocateTexture", byteDepth);
    throw std::exception("Invalid byte depth");
}

gli::texture2d PngLoader::loadPng(const fs::path& path, bool flipV) {
    LOG_INFO("Loading png {}", path.string());

    gli::texture2d res;

    FILE* rawFilePtr = nullptr;
    if (errno_t err = fopen_s(&rawFilePtr, path.string().c_str(), "rb")) {
        char errBuff[256];
        strerror_s(errBuff, err);
        panic("loadPng Unable to open file - {} with error {}", path.string(), errBuff);
    }
    // Transfer ownership to RAII file handle
    std::unique_ptr<FILE, int (*)(FILE*)> file(rawFilePtr, fclose);

    char header[8];
    fread(header, 1, 8, file.get());

    if (png_sig_cmp((png_const_bytep)header, 0, 8))
    {
        panic("loadPng File type not recognized - {}", path.string());
    }

    png_structp png_ptr;

    /* initialize stuff */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr)
    {
        panic("loadPng Format not recognized - {}", path.string());
    }

    png_infop info_ptr;

    info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr)
    {
        panic("loadPng Unable to retrieve image information - {}", path.string());
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        panic("loadPng File corrupt - {}", path.string());
    }

    int width, height;
    png_byte color_type;
    png_byte bit_depth;
    //int number_of_passes;

    // NOTE: Byte order: PNG is a network-byte-order format (big-endian), while x86/x64 architecture is little-endian.
    // This should generally be handled by libpng, but if you're using a 16-bit format, you might need to swap the byte order.
    // libpng offers the png_set_swap() function for this purpose. If you are using a 16-bit color depth (bit_depth==16), 
    // you should call png_set_swap(png_ptr) after png_read_update_info().

    png_init_io(png_ptr, file.get());
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);

    if (width > MAX_TEXTURE_DIMENSION || height > MAX_TEXTURE_DIMENSION) {
        panic("Texture {} dimensions <{},{}> greater than max of {}", path.string(), width, height, MAX_TEXTURE_DIMENSION);
    }
    // In your texture dimension check
    if (!isPowerOfTwo(width) || !isPowerOfTwo(height)) {
        panic("Texture {} dimensions <{},{}> are not power of two", path.string(), width, height);
    }

    //number_of_passes=png_set_interlace_handling(png_ptr);
    assert(color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA);

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
        color_type = png_get_color_type(png_ptr, info_ptr);
        LOG_WARN("PNG {} is palettized, make sure it works", path.string());
    }

    png_read_update_info(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        LOG_CRITICAL("loadPng File corrupt - {}", path.string());
        return res;
    }

    int channels = 0;
    int byteDepth = 0;

    if(color_type==PNG_COLOR_TYPE_GRAY)
        channels=1;
    else if(color_type==PNG_COLOR_TYPE_GRAY_ALPHA)
        channels=2;
    else if(color_type==PNG_COLOR_TYPE_RGB)
        channels=3;
    else if(color_type==PNG_COLOR_TYPE_RGB_ALPHA)
        channels=4;
    else {
        LOG_CRITICAL("Unsupported channels of {} in {}", channels, path.string());
        assert("false");
    }
    
    if(bit_depth==8)
        byteDepth = 1;
    else if(bit_depth==16)
        byteDepth = 2;
    else {
        LOG_CRITICAL("Unsupported bit depth of {} in {}", bit_depth, path.string());
        assert("false");
    }

    res = allocateTexture(width, height, byteDepth, channels);
    { // Error checks
        const int num_channels = gli::component_count(res.format());
        const int block_size = gli::block_size(res.format());
        const int byte_depth = block_size / num_channels;
        assert(channels == num_channels);
        assert(byte_depth == byteDepth);
    }
    png_byte* imageData = (png_byte*)res.data();

    size_t pos = 0;
    const size_t stride = width * channels * byteDepth;
    assert(res.size(0) == stride * height);

    png_bytep row_pointers[MAX_TEXTURE_DIMENSION];
    // Inverted on purpose as the convention is flipped
    if (!flipV) {
        for (int y = height - 1; y >= 0; y--) {
            row_pointers[y] = &imageData[pos];
            pos += stride;
        }
    }
    else {
        for (int y = 0; y < height; y++) {
            row_pointers[y] = &imageData[pos];
            pos += stride;
        }
    }

    png_read_image(png_ptr, row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

    return res;
}
