/*
 * Copyright 2025 Rive
 */
#pragma once

#include "rive/refcnt.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/renderer/d3d12/d3d12.hpp"
#include "rive/renderer/gpu_resource.hpp"
#include <iostream>

namespace rive::gpu
{

class D3D12Resource;
// print the descriptor of the root signature
void print_sig_descriptor(LPCVOID data, SIZE_T size);
#if defined(DEBUG)
// Assign a name to the object to aid with debugging.
void SetName(D3D12Resource* pObject, LPCWSTR name);
#define NAME_D3D12_OBJECT(x) SetName(x.get(), L#x)
#define RNAME_D3D12_OBJECT(x, s)                                               \
    std::wstringstream ss;                                                     \
    ss << L#x;                                                                 \
    ss << " " << s;                                                            \
    SetName(x, ss.str().c_str())
#define SNAME_D3D12_OBJECT(x, s) SetName(x.get(), L##s)
#define VNAME_D3D12_OBJECT(x) SetName(x->resource(), L#x)
#define SVNAME_D3D12_OBJECT(x, s) SetName(x->resource(), L##s)
#define NAME_RAW_D3D12_OBJECT(x) x->SetName(L#x)
#else
#define NAME_D3D12_OBJECT(x)
#define RNAME_D3D12_OBJECT(x, s)
#define SNAME_D3D12_OBJECT(x, s)
#define VNAME_D3D12_OBJECT(x)
#define SVNAME_D3D12_OBJECT(x, s)
#define NAME_RAW_D3D12_OBJECT(x)
#endif

class D3D12Buffer;
class D3D12Texture;
class D3D12TextureArray;
class D3D12DescriptorHeap : public GPUResource
{
public:
    D3D12DescriptorHeap(rcp<GPUResourceManager> manager,
                        ID3D12Device* device,
                        UINT numDescriptors,
                        D3D12_DESCRIPTOR_HEAP_TYPE type,
                        D3D12_DESCRIPTOR_HEAP_FLAGS flags);

    ID3D12DescriptorHeap* heap() const { return m_heap.Get(); }

    void markSamplerToIndex(ID3D12Device* device,
                            const D3D12_SAMPLER_DESC& desc,
                            UINT index);
    void markCbvToIndex(ID3D12Device* device,
                        D3D12Buffer* resource,
                        UINT index,
                        UINT sizeInBytes,
                        SIZE_T offset = 0);
    void markSrvToIndex(ID3D12Device* device,
                        D3D12Buffer* resource,
                        UINT index,
                        UINT numElements,
                        UINT elementByteStride,
                        UINT64 firstElement);
    void markSrvToIndex(ID3D12Device* device,
                        D3D12Buffer* resource,
                        UINT index,
                        UINT numElements,
                        UINT elementByteStride,
                        UINT gpuByteStride,
                        UINT64 firstElement);
    void markUavToIndex(ID3D12Device* device,
                        D3D12Buffer* resource,
                        DXGI_FORMAT format,
                        UINT index,
                        UINT numElements,
                        UINT elementByteStride);
    void markSrvToIndex(ID3D12Device* device,
                        D3D12Texture* resource,
                        UINT index);
    void markSrvToIndex(ID3D12Device* device,
                        D3D12TextureArray* resource,
                        UINT index);
    void markUavToIndex(ID3D12Device* device,
                        D3D12Texture* resource,
                        DXGI_FORMAT format,
                        UINT index);
    void markRtvToIndex(ID3D12Device* device,
                        D3D12Texture* resource,
                        UINT index);

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleForUpload(UINT index) const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_heap->GetCPUDescriptorHandleForHeapStart(),
            index,
            m_heapDescriptorSize);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandleForIndex(UINT index) const
    {
        assert(m_flags == D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            m_heap->GetCPUDescriptorHandleForHeapStart(),
            index,
            m_heapDescriptorSize);
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleForIndex(UINT index) const
    {
        assert(m_flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_heap->GetGPUDescriptorHandleForHeapStart(),
            index,
            m_heapDescriptorSize);
    }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
#ifndef NDEBUG
    const D3D12_DESCRIPTOR_HEAP_TYPE m_type;
    const D3D12_DESCRIPTOR_HEAP_FLAGS m_flags;
#endif
    const UINT m_heapDescriptorSize;
};

class D3D12Resource : public GPUResource
{
public:
    virtual ~D3D12Resource() {}

    ID3D12Resource* resource() const { return m_resource.Get(); }
    const D3D12_RESOURCE_DESC& desc() const { return m_desc; }

#if DEBUG
    LPCWSTR m_name = L"";
    void SetName(LPCWSTR name)
    {
        m_name = name;
        m_resource->SetName(name);
    }
#endif

protected:
    D3D12Resource(rcp<GPUResourceManager> manager,
                  ComPtr<ID3D12Resource> resource,
                  D3D12_RESOURCE_STATES initialState);

    D3D12Resource(rcp<GPUResourceManager> manager,
                  ID3D12Device* device,
                  D3D12_RESOURCE_STATES initialState,
                  D3D12_HEAP_FLAGS heapFlags,
                  const D3D12_RESOURCE_DESC& desc,
                  const D3D12_CLEAR_VALUE* clearValue = nullptr);

    D3D12Resource(rcp<GPUResourceManager> manager,
                  ID3D12Device* device,
                  D3D12_RESOURCE_STATES initialState,
                  D3D12_HEAP_FLAGS heapFlags,
                  CD3DX12_HEAP_PROPERTIES heapPropeties,
                  const D3D12_RESOURCE_DESC& desc,
                  const D3D12_CLEAR_VALUE* clearValue = nullptr);

    // the gpu side resource
    ComPtr<ID3D12Resource> m_resource;
    // used for barrier state tracking
    D3D12_RESOURCE_STATES m_lastState;
    D3D12_RESOURCE_DESC m_desc;
    CD3DX12_HEAP_PROPERTIES m_heapPropeties;
    D3D12_HEAP_FLAGS m_heapFlags;

    friend class D3D12ResourceManager;
};

class D3D12Texture : public D3D12Resource
{
public:
    D3D12Texture(
        rcp<GPUResourceManager> manager,
        ComPtr<ID3D12Resource> resource,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON) :
        D3D12Resource(manager, resource, initialState)
    {}

    D3D12Texture(rcp<GPUResourceManager> manager,
                 ID3D12Device* device,
                 D3D12_RESOURCE_STATES initialState,
                 D3D12_HEAP_FLAGS heapFlags,
                 const D3D12_RESOURCE_DESC& desc,
                 const D3D12_CLEAR_VALUE* clearValue = nullptr) :
        D3D12Resource(manager,
                      device,
                      initialState,
                      heapFlags,
                      desc,
                      clearValue)
    {}

    D3D12Texture(rcp<GPUResourceManager> manager,
                 ID3D12Device* device,
                 D3D12_RESOURCE_STATES initialState,
                 D3D12_HEAP_FLAGS heapFlags,
                 CD3DX12_HEAP_PROPERTIES heapPropeties,
                 const D3D12_RESOURCE_DESC& desc,
                 const D3D12_CLEAR_VALUE* clearValue = nullptr) :
        D3D12Resource(manager,
                      device,
                      initialState,
                      heapFlags,
                      heapPropeties,
                      desc,
                      clearValue)
    {}

    // helpers
    UINT64 width() const { return m_desc.Width; }
    UINT64 height() const { return m_desc.Height; }
    DXGI_FORMAT format() const { return m_desc.Format; }

    // USE WITH CAUTION !! this force release the ID3D12Resource held by this
    // texture. Be sure that it is no longer needed or in any active command
    // list
    void forceReleaseResource() { m_resource.Reset(); }
};

class D3D12TextureArray : public D3D12Resource
{
public:
    D3D12TextureArray(
        rcp<GPUResourceManager> manager,
        ComPtr<ID3D12Resource> resource,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON) :
        D3D12Resource(manager, resource, initialState)
    {}

    D3D12TextureArray(rcp<GPUResourceManager> manager,
                      ID3D12Device* device,
                      D3D12_RESOURCE_STATES initialState,
                      D3D12_HEAP_FLAGS heapFlags,
                      const D3D12_RESOURCE_DESC& desc) :
        D3D12Resource(manager, device, initialState, heapFlags, desc)
    {}

    D3D12TextureArray(rcp<GPUResourceManager> manager,
                      ID3D12Device* device,
                      D3D12_RESOURCE_STATES initialState,
                      D3D12_HEAP_FLAGS heapFlags,
                      CD3DX12_HEAP_PROPERTIES heapPropeties,
                      const D3D12_RESOURCE_DESC& desc) :
        D3D12Resource(manager,
                      device,
                      initialState,
                      heapFlags,
                      heapPropeties,
                      desc)
    {}

    // helpers
    UINT64 width() const { return m_desc.Width; }
    UINT16 length() const { return m_desc.DepthOrArraySize; }
    DXGI_FORMAT format() const { return m_desc.Format; }
};

class D3D12Buffer : public D3D12Resource
{
public:
    D3D12Buffer(rcp<GPUResourceManager> manager,
                ID3D12Device* device,
                D3D12_RESOURCE_STATES initialState,
                D3D12_HEAP_FLAGS heapFlags,
                UINT size,
                const D3D12_RESOURCE_DESC& desc) :
        D3D12Resource(manager, device, initialState, heapFlags, desc),
        m_sizeInBytes(size)
    {}

    D3D12Buffer(rcp<GPUResourceManager> manager,
                ID3D12Device* device,
                D3D12_RESOURCE_STATES initialState,
                D3D12_HEAP_FLAGS heapFlags,
                CD3DX12_HEAP_PROPERTIES heapPropeties,
                UINT size,
                const D3D12_RESOURCE_DESC& desc) :
        D3D12Resource(manager,
                      device,
                      initialState,
                      heapFlags,
                      heapPropeties,
                      desc),
        m_sizeInBytes(size)
    {}

    void* map();
    void unmap();

    UINT sizeInBytes() const { return m_sizeInBytes; }

    D3D12_GPU_VIRTUAL_ADDRESS getGPUVirtualAddress() const
    {
        return m_resource->GetGPUVirtualAddress();
    }

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView(uint32_t strideInBytes) const
    {
        return vertexBufferView(0,
                                m_sizeInBytes / strideInBytes,
                                strideInBytes);
    }

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView(uint32_t firstElement,
                                              uint32_t strideInBytes) const
    {
        return vertexBufferView(firstElement,
                                (m_sizeInBytes / strideInBytes) - firstElement,
                                strideInBytes);
    }

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView(uint32_t firstElement,
                                              uint32_t numElements,
                                              uint32_t strideInBytes) const
    {
        D3D12_VERTEX_BUFFER_VIEW VBView;
        VBView.BufferLocation =
            resource()->GetGPUVirtualAddress() + (firstElement * strideInBytes);
        VBView.SizeInBytes = numElements * strideInBytes;
        VBView.StrideInBytes = strideInBytes;
        return VBView;
    }

    D3D12_INDEX_BUFFER_VIEW indexBufferView() const
    {
        return indexBufferView(0, m_sizeInBytes);
    }

    D3D12_INDEX_BUFFER_VIEW indexBufferView(size_t Offset,
                                            uint32_t sizeInBytes) const
    {
        D3D12_INDEX_BUFFER_VIEW IBView;
        IBView.BufferLocation = resource()->GetGPUVirtualAddress() + Offset;
        IBView.SizeInBytes = sizeInBytes;
        IBView.Format = DXGI_FORMAT_R16_UINT;
        return IBView;
    }

private:
    UINT m_sizeInBytes;
#ifndef NDEBUG
    bool m_isMapped = false;
#endif
};
class D3D12ResourceManager;
// A buffer that is intended to be updated every frame
class D3D12VolatileBuffer : public GPUResource
{
public:
    // create buffers with intialSize, initialSize must not be 0
    D3D12VolatileBuffer(
        rcp<D3D12ResourceManager> manager,
        UINT initialSize,
        D3D12_RESOURCE_FLAGS bindFlags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

    // maps the upload buffer
    void* map(size_t mapSizeInBytes)
    {
        m_lastMapSize = mapSizeInBytes;
        assert(m_uploadBuffer);
        return m_uploadBuffer->map();
    }
    // unmaps the upload buffer
    void unmap(size_t unmapSizeInBytes)
    {
        assert(unmapSizeInBytes == m_lastMapSize);
        assert(m_uploadBuffer);
        m_uploadBuffer->unmap();
    }
    // sync the upload buffer to the gpu buffer using m_lastMapSize as
    // bytesToSync
    void sync(ID3D12GraphicsCommandList* cmdList, UINT64 offsetBytes);
    // sync the upload buffer to the gpu buffer
    void sync(ID3D12GraphicsCommandList* cmdList,
              UINT64 offsetBytes,
              UINT64 bytesToSync);

    void syncToBuffer(ID3D12GraphicsCommandList* cmdList,
                      D3D12Buffer* buffer,
                      UINT64 offsetBytes,
                      UINT64 bytesToSync) const;

    D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress() const
    {
        return m_gpuBuffer->resource()->GetGPUVirtualAddress();
    }
    D3D12Buffer* resource() const { return m_gpuBuffer.get(); }
    UINT64 sizeInBytes() const
    {
        assert(m_uploadBuffer->sizeInBytes() == m_gpuBuffer->sizeInBytes());
        return m_uploadBuffer->sizeInBytes();
    }

    // recreates the buffers with given size, there is no way to just inflate or
    // deflate a buffer
    void resizeBuffers(UINT newSize);

private:
    D3D12ResourceManager* d3d() const;

    rcp<D3D12Buffer> m_uploadBuffer;
    rcp<D3D12Buffer> m_gpuBuffer;

    // used for creating gpu buffers
    D3D12_RESOURCE_FLAGS m_bindFlags;
    D3D12_HEAP_FLAGS m_heapFlags;

    UINT64 m_lastMapSize = 0;
};

class D3D12ResourceManager : public GPUResourceManager
{
public:
    D3D12ResourceManager(ComPtr<ID3D12Device> device) :
        m_device(std::move(device))
    {}

    rcp<D3D12DescriptorHeap> makeHeap(UINT numDescriptors,
                                      D3D12_DESCRIPTOR_HEAP_TYPE type,
                                      D3D12_DESCRIPTOR_HEAP_FLAGS flags);
    rcp<D3D12TextureArray> make1DTextureArray(
        UINT width,
        UINT16 length,
        UINT mipLevelCount,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS bindFlags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

    rcp<D3D12Texture> make2DTexture(
        UINT width,
        UINT height,
        UINT mipLevelCount,
        DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS bindFlags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE,
        const D3D12_CLEAR_VALUE* clearValue = nullptr);

    rcp<D3D12Buffer> makeBuffer(
        UINT size,
        D3D12_RESOURCE_FLAGS bindFlags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

    rcp<D3D12Buffer> makeUploadBuffer(
        UINT size,
        D3D12_RESOURCE_FLAGS bindFlags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

    rcp<D3D12VolatileBuffer> makeVolatileBuffer(
        UINT size,
        D3D12_RESOURCE_FLAGS bindFlags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

    template <typename DataType, size_t N>
    rcp<D3D12Buffer> makeImmutableBuffer(ID3D12GraphicsCommandList* cmdList,
                                         const DataType (&data)[N])
    {
        constexpr size_t sizeInBytes = N * sizeof(DataType);
        rcp<D3D12Buffer> uploadBuffer =
            makeUploadBuffer(sizeInBytes,
                             D3D12_RESOURCE_FLAG_NONE,
                             D3D12_RESOURCE_STATE_GENERIC_READ);
        rcp<D3D12Buffer> constBuffer = makeBuffer(sizeInBytes,
                                                  D3D12_RESOURCE_FLAG_NONE,
                                                  D3D12_RESOURCE_STATE_COMMON);

        void* ptr = uploadBuffer->map();
        memcpy(ptr, data, sizeInBytes);
        uploadBuffer->unmap();

        cmdList->CopyBufferRegion(constBuffer->resource(),
                                  0,
                                  uploadBuffer->resource(),
                                  0,
                                  sizeInBytes);

        return constBuffer;
    }

    rcp<D3D12Texture> makeExternalTexture(
        ComPtr<ID3D12Resource> externalTexture,
        D3D12_RESOURCE_STATES lastState);
    // transition resource to "toState"
    void transition(ID3D12GraphicsCommandList* cmdList,
                    D3D12Resource* resource,
                    D3D12_RESOURCE_STATES toState);
    void clearUAV(ID3D12GraphicsCommandList* cmdList,
                  ID3D12Resource* resource,
                  CD3DX12_GPU_DESCRIPTOR_HANDLE&,
                  CD3DX12_CPU_DESCRIPTOR_HANDLE&,
                  const UINT clearColor[4],
                  bool needsBarrier);
    void clearUAV(ID3D12GraphicsCommandList* cmdList,
                  ID3D12Resource* resource,
                  CD3DX12_GPU_DESCRIPTOR_HANDLE&,
                  CD3DX12_CPU_DESCRIPTOR_HANDLE&,
                  const float clearColor[4],
                  bool needsBarrier);

    ID3D12Device* device() const { return m_device.Get(); }

private:
    ComPtr<ID3D12Device> m_device;
};

class D3D12VolatileBufferPool : public GPUResourcePool
{
public:
    D3D12VolatileBufferPool(rcp<D3D12ResourceManager> manager,
                            UINT alignment = 1,
                            UINT size = 0);

    size_t size() const { return m_targetSize; }
    void setTargetSize(size_t size);

    // Returns a Buffer that is guaranteed to exist and be of size
    // 'm_targetSize'.
    rcp<D3D12VolatileBuffer> acquire();

    void recycle(rcp<D3D12VolatileBuffer> resource)
    {
        GPUResourcePool::recycle(std::move(resource));
    }

private:
    D3D12ResourceManager* d3d() const;

    constexpr static size_t MAX_POOL_SIZE = 8;
    UINT m_targetSize;
    UINT m_alignment;
};

class D3D12DescriptorHeapPool : public GPUResourcePool
{
public:
    D3D12DescriptorHeapPool(rcp<D3D12ResourceManager> manager,
                            UINT numDescriptors,
                            D3D12_DESCRIPTOR_HEAP_TYPE type,
                            D3D12_DESCRIPTOR_HEAP_FLAGS flags);

    rcp<D3D12DescriptorHeap> acquire();
    void recycle(rcp<D3D12DescriptorHeap> resource)
    {
        GPUResourcePool::recycle(std::move(resource));
    }

private:
    D3D12ResourceManager* d3d() const;

    constexpr static size_t MAX_POOL_SIZE = 8;
    // heap pools do not change over time, they are expected to be initialized
    // with the max number of descriptors ever used
    const UINT m_numDescriptors;
    const D3D12_DESCRIPTOR_HEAP_TYPE m_type;
    const D3D12_DESCRIPTOR_HEAP_FLAGS m_flags;
};
} // namespace rive::gpu