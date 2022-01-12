//
// ImageIO.h
// Vorb Engine
//
// Created by Cristian Zaloj on 8 Dec 2014
// Copyright 2014 Regrowth Studios
// MIT License
//

/*! \file ImageIO.h
 * @brief 
 */

#pragma once

#ifndef Vorb_ImageIO_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_ImageIO_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

#include "../Event.hpp"
#include "../io/Path.h"
#include "BitmapResource.h"

namespace vorb {
    namespace graphics {
        enum class ImageIOFormat {
            RAW = 0,
            RGB_UI8,
            RGBA_UI8,
            RGB_UI16,
            RGBA_UI16,
            RGB_F32,
            RGBA_F32,
            RGB_F64,
            RGBA_F64
        };
        class ImageIO;

        class ImageIO {
        public:
            static BitmapResource alloc(const ui32& w, const ui32& h, const ImageIOFormat& format);
            static void free(BitmapResource& res);

            BitmapResource load(const vio::Path& path,
                                const ImageIOFormat& format = ImageIOFormat::RGBA_UI8,
                                bool flipV = false);
            bool save(const vio::Path& path, const void* inData, const ui32& w,
                      const ui32& h, const ImageIOFormat& format);

            Event<nString> onError;
        };

        /*class ScopedPNGLoader {
        public:
            ScopedPNGLoader(const vio::Path& path, const ImageIOFormat& format = ImageIOFormat::RGBA_UI8);
            ~ScopedPNGLoader();

            ui32 getWidth() const { return mDims.x; }
            ui32 getHeight() const { return mDims.y; }
            const ui32v2& getDims() const { return mDims; }

            void loadIntoDestination(unsigned char* dst, size_t rowSize);

        private:
            nString mError;
            ImageIOFormat mFormat;
            ui32v2 mDims;
            FILE* mFile;
            void* mPngPtr;
        };*/

        /// Destroys the resource in the destructor
        class ScopedBitmapResource : public BitmapResource {
        public:
            ScopedBitmapResource() {};
            ScopedBitmapResource(const BitmapResource& rs) {
//                memcpy(this, &rs, sizeof(BitmapResource));
                this->width=rs.width;
                this->height=rs.height;
                this->data=rs.data;
            }
            virtual ~ScopedBitmapResource() {
                ImageIO::free(*this);
            }
            ScopedBitmapResource& operator=(const BitmapResource& rs) {
//                memcpy(this, &rs, sizeof(BitmapResource));
                this->width=rs.width;
                this->height=rs.height;
                this->data=rs.data;
                return *this;
            }
        private:
            VORB_NON_COPYABLE(ScopedBitmapResource);
        };
    }
}
namespace vg = vorb::graphics;

#endif // !Vorb_ImageIO_h__
