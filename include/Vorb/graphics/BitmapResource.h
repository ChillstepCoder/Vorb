#pragma once

namespace vorb {
    namespace graphics {

        class BitmapResource {
            friend class ImageIO;
        public:
            BitmapResource() :width(0), height(0), data(nullptr) {}
            virtual ~BitmapResource() {};

            ui32 width;
            ui32 height;
            union {
                void* data;
                ui8* bytesUI8;
                ui8v2* bytesUI8v2;
                ui8v3* bytesUI8v3;
                ui8v4* bytesUI8v4;
                ui16* bytesUI16;
                ui16v2* bytesUI16v2;
                ui16v3* bytesUI16v3;
                ui16v4* bytesUI16v4;
                f32* bytesF32;
                f32v2* bytesF32v2;
                f32v3* bytesF32v3;
                f32v4* bytesF32v4;
                f64* bytesF64;
                f64v2* bytesF64v2;
                f64v3* bytesF64v3;
                f64v4* bytesF64v4;
            };
        };


    }
}
namespace vg = vorb::graphics;