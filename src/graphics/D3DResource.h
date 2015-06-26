//
// D3DResource.h
// Vorb Engine
//
// Created by Cristian Zaloj on 2 Jun 2015
// Copyright 2014 Regrowth Studios
// All Rights Reserved
//

/*! \file D3DResource.h
 * @brief 
 */

#pragma once

#ifndef Vorb_D3DResource_h__
//! @cond DOXY_SHOW_HEADER_GUARDS
#define Vorb_D3DResource_h__
//! @endcond

#ifndef VORB_USING_PCH
#include "../types.h"
#endif // !VORB_USING_PCH

#include <d3d11.h>

#include "graphics/IResource.h"

namespace vorb {
    namespace graphics {

        class D3DBuffer : public IBuffer {
        public:
            D3DBuffer(IContext* owner) : IBuffer(owner) {
                // Empty
            }

            virtual void disposeInternal() override {
                if(data) data->Release();
            }

            virtual size_t getMemoryUsed() const override {
                return size;
            }

            ID3D11Buffer* data = nullptr;
            size_t size = 0;
        };

        class D3DConstantBlock : public IConstantBlock {
        public:
            D3DConstantBlock(IContext* owner) : IConstantBlock(owner) {
                // Empty
            }

            virtual void disposeInternal() override {
                if (data) data->Release();
            }

            virtual size_t getMemoryUsed() const override {
                return size;
            }

            ID3D11Buffer* data = nullptr;
            size_t size = 0;
        };

        class D3DTexture1D : public ITexture1D {
        public:
            D3DTexture1D(IContext* owner) : ITexture1D(owner) {
                // Empty
            }

            virtual void disposeInternal() override {
                if (data) data->Release();
            }

            virtual size_t getMemoryUsed() const override {
                return size;
            }

            ui32 arraySlices = 0;
            ID3D11Texture1D* data = nullptr;
            size_t size = 0;
        };

        class D3DTexture2D : public ITexture2D {
        public:
            D3DTexture2D(IContext* owner) : ITexture2D(owner) {
                // Empty
            }

            virtual void disposeInternal() override {
                if (data) data->Release();
            }

            virtual size_t getMemoryUsed() const override {
                return size;
            }

            ui32 arraySlices = 0;
            ID3D11Texture2D* data = nullptr;
            size_t size = 0;
        };

        class D3DTexture3D : public ITexture3D {
        public:
            D3DTexture3D(IContext* owner) : ITexture3D(owner) {
                // Empty
            }

            virtual void disposeInternal() override {
                if (data) data->Release();
            }

            virtual size_t getMemoryUsed() const override {
                return size;
            }

            ID3D11Texture3D* data = nullptr;
            size_t size = 0;
        };

        class D3DShaderCode : public IShaderCode {
        public:
            D3DShaderCode(IContext* owner) : IShaderCode(owner) {
                // Empty
            }

            virtual const void* getCode() const override {
                return data;
            }
            virtual size_t getLength() const override {
                return size;
            }

            virtual size_t getMemoryUsed() const override {
                return 0;
            }

            virtual void disposeInternal() override {
                delete(data);
            }

            virtual vg::ShaderType getType() const {
                return type;
            }

            vg::ShaderType type = vg::ShaderType::NONE;
            void* data = nullptr;
            size_t size = 0;
        };
        class D3DShaderCodeBlob : public IShaderCode {
        public:
            D3DShaderCodeBlob(IContext* owner) : IShaderCode(owner) {
                // Empty
            }

            virtual const void* getCode() const override {
                return shaderBlob->GetBufferPointer();
            }
            virtual size_t getLength() const override {
                return shaderBlob->GetBufferSize();
            }

            virtual size_t getMemoryUsed() const override {
                return shaderBlob->GetBufferSize();
            }

            virtual void disposeInternal() override {
                shaderBlob->Release();
            }

            ID3DBlob* shaderBlob = nullptr;
        };
        class D3DVertexShader : public IVertexShader {
        public:
            D3DVertexShader(IContext* owner) : IVertexShader(owner) {
                // Empty
            }

            virtual size_t getMemoryUsed() const {
                throw std::logic_error("The method or operation is not implemented.");
            }

            virtual void disposeInternal() {
                shader->Release();
            }

            ID3D11VertexShader* shader;
        };
        class D3DGeometryShader : public IGeometryShader {
        public:
            D3DGeometryShader(IContext* owner) : IGeometryShader(owner) {
                // Empty
            }

            virtual size_t getMemoryUsed() const {
                throw std::logic_error("The method or operation is not implemented.");
            }

            virtual void disposeInternal() {
                shader->Release();
            }

            ID3D11GeometryShader* shader;
        };
        class D3DTessGenShader : public ITessGenShader {
        public:
            D3DTessGenShader(IContext* owner) : ITessGenShader(owner) {
                // Empty
            }

            virtual size_t getMemoryUsed() const {
                throw std::logic_error("The method or operation is not implemented.");
            }

            virtual void disposeInternal() {
                shader->Release();
            }

            ID3D11HullShader* shader;
        };
        class D3DTessEvalShader : public ITessEvalShader {
        public:
            D3DTessEvalShader(IContext* owner) : ITessEvalShader(owner) {
                // Empty
            }

            virtual size_t getMemoryUsed() const {
                throw std::logic_error("The method or operation is not implemented.");
            }

            virtual void disposeInternal() {
                shader->Release();
            }

            ID3D11DomainShader* shader;
        };
        class D3DPixelShader : public IPixelShader {
        public:
            D3DPixelShader(IContext* owner) : IPixelShader(owner) {
                // Empty
            }

            virtual size_t getMemoryUsed() const override {
                throw std::logic_error("The method or operation is not implemented.");
            }

            virtual void disposeInternal() override {
                if (shader) shader->Release();
            }

            ID3D11PixelShader* shader;
        };
        class D3DComputeShader : public IComputeShader {
        public:
            D3DComputeShader(IContext* owner) : IComputeShader(owner) {
                // Empty
            }

            virtual size_t getMemoryUsed() const {
                throw std::logic_error("The method or operation is not implemented.");
            }

            virtual void disposeInternal() {
                shader->Release();
            }

            ID3D11ComputeShader* shader;
        };
        class D3DShaderResourceView : public IBufferView, public IConstantBlockView, public ITexture1DView, public ITexture2DView, public ITexture3DView {
        public:
            D3DShaderResourceView(IContext* owner) : IBufferView(owner), IConstantBlockView(owner), ITexture1DView(owner), ITexture2DView(owner), ITexture3DView(owner) {
                // Empty
            }

            virtual void disposeInternal() {
                view->Release();
            }

            ID3D11ShaderResourceView* view;
        };
    }
}

#endif // !Vorb_D3DResource_h__