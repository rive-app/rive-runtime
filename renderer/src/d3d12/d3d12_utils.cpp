/*
 * Copyright 2025 Rive
 */
#include "rive/renderer/d3d12/d3d12_utils.hpp"

namespace rive::gpu
{
void print_sig_descriptor(LPCVOID data, SIZE_T size)
{
    ComPtr<ID3D12RootSignatureDeserializer> descS;
    D3D12CreateRootSignatureDeserializer(data, size, IID_PPV_ARGS(&descS));
    auto desc = descS->GetRootSignatureDesc();
    printf("desc {\n");
    for (size_t i = 0; i < desc->NumParameters; ++i)
    {
        const char* type;
        switch (desc->pParameters[i].ParameterType)
        {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                type = "table";
                break;
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
                type = "constant";
                break;
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
                type = "cbv";
                break;
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
                type = "srv";
                break;
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
                type = "uav";
                break;
            default:
                type = "unkown";
        }

        printf("root desc name %s\n", type);
    }
    printf("}\n");
}
#if defined(DEBUG)
void SetName(D3D12Resource* pObject, LPCWSTR name) { pObject->SetName(name); }
#else
void printDescriptor(LPCVOID data, SIZE_T size) {}
void SetName(ID3D12Object*, LPCWSTR) {}
#endif

D3D12DescriptorHeap::D3D12DescriptorHeap(rcp<GPUResourceManager> manager,
                                         ID3D12Device* device,
                                         UINT numDescriptors,
                                         D3D12_DESCRIPTOR_HEAP_TYPE type,
                                         D3D12_DESCRIPTOR_HEAP_FLAGS flags) :
    GPUResource(std::move(manager)),
#ifndef NDEBUG
    m_type(type),
    m_flags(flags),
#endif
    m_heapDescriptorSize(device->GetDescriptorHandleIncrementSize(type))
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = flags;

    VERIFY_OK(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));
}

void D3D12DescriptorHeap::markSamplerToIndex(ID3D12Device* device,
                                             const D3D12_SAMPLER_DESC& desc,
                                             UINT index)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    CD3DX12_CPU_DESCRIPTOR_HANDLE samplerHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateSampler(&desc, samplerHandle);
}

void D3D12DescriptorHeap::markCbvToIndex(ID3D12Device* device,
                                         D3D12Buffer* resource,
                                         UINT index,
                                         UINT sizeInBytes,
                                         SIZE_T offset)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.SizeInBytes = sizeInBytes;
    cbvDesc.BufferLocation =
        resource->resource()->GetGPUVirtualAddress() + offset;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateConstantBufferView(&cbvDesc, cbvHandle);
}

void D3D12DescriptorHeap::markSrvToIndex(ID3D12Device* device,
                                         D3D12Buffer* resource,
                                         UINT index,
                                         UINT numElements,
                                         UINT elementByteStride,
                                         UINT64 firstElement)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = firstElement;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = elementByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Format = resource->desc().Format;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateShaderResourceView(resource->resource(), &srvDesc, srvHandle);
}

void D3D12DescriptorHeap::markSrvToIndex(ID3D12Device* device,
                                         D3D12Buffer* resource,
                                         UINT index,
                                         UINT numElements,
                                         UINT elementByteStride,
                                         UINT gpuByteStride,
                                         UINT64 firstElement)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    assert(elementByteStride % gpuByteStride == 0);

    const UINT cpuScale = elementByteStride / gpuByteStride;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = firstElement * cpuScale;
    srvDesc.Buffer.NumElements = numElements * cpuScale;
    srvDesc.Buffer.StructureByteStride = gpuByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvDesc.Format = resource->desc().Format;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateShaderResourceView(resource->resource(), &srvDesc, srvHandle);
}

void D3D12DescriptorHeap::markUavToIndex(ID3D12Device* device,
                                         D3D12Buffer* resource,
                                         DXGI_FORMAT format,
                                         UINT index,
                                         UINT numElements,
                                         UINT elementByteStride)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.StructureByteStride = elementByteStride;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uavDesc.Format = format;

    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateUnorderedAccessView(resource->resource(),
                                      nullptr,
                                      &uavDesc,
                                      uavHandle);
}

void D3D12DescriptorHeap::markSrvToIndex(ID3D12Device* device,
                                         D3D12Texture* resource,
                                         UINT index)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = resource->desc().MipLevels;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Format = resource->desc().Format;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateShaderResourceView(resource->resource(), &srvDesc, srvHandle);
}

void D3D12DescriptorHeap::markSrvToIndex(ID3D12Device* device,
                                         D3D12TextureArray* resource,
                                         UINT index)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
    srvDesc.Texture1DArray.MipLevels = resource->desc().MipLevels;
    srvDesc.Texture1DArray.ArraySize = resource->length();
    srvDesc.Texture1DArray.FirstArraySlice = 0;
    srvDesc.Format = resource->desc().Format;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateShaderResourceView(resource->resource(), &srvDesc, srvHandle);
}

void D3D12DescriptorHeap::markUavToIndex(ID3D12Device* device,
                                         D3D12Texture* resource,
                                         DXGI_FORMAT format,
                                         UINT index)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;
    uavDesc.Format = format;

    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);

    device->CreateUnorderedAccessView(resource->resource(),
                                      nullptr,
                                      &uavDesc,
                                      uavHandle);
}

void D3D12DescriptorHeap::markRtvToIndex(ID3D12Device* device,
                                         D3D12Texture* resource,
                                         UINT index)
{
    assert(m_type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    RTVDesc.Texture2D.MipSlice = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_heap->GetCPUDescriptorHandleForHeapStart(),
        index,
        m_heapDescriptorSize);
    device->CreateRenderTargetView(resource->resource(), &RTVDesc, rtvHandle);
}

D3D12Resource::D3D12Resource(rcp<GPUResourceManager> manager,
                             ComPtr<ID3D12Resource> resource,
                             D3D12_RESOURCE_STATES initialState) :
    GPUResource(std::move(manager)),
    m_resource(resource),
    m_lastState(initialState),
    m_desc(m_resource->GetDesc()),
    m_heapPropeties(D3D12_HEAP_TYPE_DEFAULT),
    m_heapFlags(D3D12_HEAP_FLAG_NONE)
{}

D3D12Resource::D3D12Resource(rcp<GPUResourceManager> manager,
                             ID3D12Device* device,
                             D3D12_RESOURCE_STATES initialState,
                             D3D12_HEAP_FLAGS heapFlags,
                             const D3D12_RESOURCE_DESC& resourceDesc,
                             const D3D12_CLEAR_VALUE* clearValue) :
    GPUResource(std::move(manager)),
    m_lastState(initialState),
    m_desc(std::move(resourceDesc)),
    m_heapFlags(heapFlags)
{
    m_heapPropeties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    VERIFY_OK(device->CreateCommittedResource(&m_heapPropeties,
                                              heapFlags,
                                              &m_desc,
                                              m_lastState,
                                              clearValue,
                                              IID_PPV_ARGS(&m_resource)));
}

D3D12Resource::D3D12Resource(rcp<GPUResourceManager> manager,
                             ID3D12Device* device,
                             D3D12_RESOURCE_STATES initialState,
                             D3D12_HEAP_FLAGS heapFlags,
                             CD3DX12_HEAP_PROPERTIES heapPropeties,
                             const D3D12_RESOURCE_DESC& desc,
                             const D3D12_CLEAR_VALUE* clearValue) :
    GPUResource(std::move(manager)),
    m_lastState(initialState),
    m_desc(std::move(desc)),
    m_heapPropeties(heapPropeties),
    m_heapFlags(heapFlags)
{
    VERIFY_OK(device->CreateCommittedResource(&m_heapPropeties,
                                              heapFlags,
                                              &m_desc,
                                              m_lastState,
                                              clearValue,
                                              IID_PPV_ARGS(&m_resource)));
}

void* D3D12Buffer::map()
{
    assert(m_sizeInBytes);
    assert(!m_isMapped);
    assert(m_heapPropeties.IsCPUAccessible());
    void* data = nullptr;
    VERIFY_OK(m_resource->Map(0, nullptr, &data));
#ifndef NDEBUG
    m_isMapped = true;
#endif
    return data;
}

void D3D12Buffer::unmap()
{
    assert(m_isMapped);
    assert(m_heapPropeties.IsCPUAccessible());
    m_resource->Unmap(0, nullptr);
#ifndef NDEBUG
    m_isMapped = false;
#endif
}

D3D12VolatileBuffer::D3D12VolatileBuffer(rcp<D3D12ResourceManager> manager,
                                         UINT initialSize,
                                         D3D12_RESOURCE_FLAGS bindFlags,
                                         D3D12_HEAP_FLAGS heapFlags) :
    GPUResource(std::move(manager)),
    m_bindFlags(bindFlags),
    m_heapFlags(heapFlags)
{
    assert(initialSize > 0);
    resizeBuffers(initialSize);
}

D3D12ResourceManager* D3D12VolatileBuffer::d3d() const
{
    return static_cast<D3D12ResourceManager*>(m_manager.get());
}

void D3D12VolatileBuffer::sync(ID3D12GraphicsCommandList* cmdList,
                               UINT64 offsetBytes)
{
    assert(m_uploadBuffer->sizeInBytes() == m_gpuBuffer->sizeInBytes());
    sync(cmdList, offsetBytes, m_uploadBuffer->sizeInBytes());
}

void D3D12VolatileBuffer::sync(ID3D12GraphicsCommandList* cmdList,
                               UINT64 offsetBytes,
                               UINT64 bytesToSync)
{
    assert(bytesToSync);
    d3d()->transition(cmdList,
                      m_gpuBuffer.get(),
                      D3D12_RESOURCE_STATE_COPY_DEST);

    cmdList->CopyBufferRegion(m_gpuBuffer->resource(),
                              0,
                              m_uploadBuffer->resource(),
                              offsetBytes,
                              bytesToSync);
    d3d()->transition(cmdList, m_gpuBuffer.get(), D3D12_RESOURCE_STATE_COMMON);
}

void D3D12VolatileBuffer::syncToBuffer(ID3D12GraphicsCommandList* cmdList,
                                       D3D12Buffer* buffer,
                                       UINT64 offsetBytes,
                                       UINT64 bytesToSync) const
{
    assert(buffer->sizeInBytes() >= bytesToSync);

    cmdList->CopyBufferRegion(buffer->resource(),
                              0,
                              m_uploadBuffer->resource(),
                              offsetBytes,
                              bytesToSync);
}

void D3D12VolatileBuffer::resizeBuffers(UINT newSize)
{
    m_uploadBuffer = d3d()->makeUploadBuffer(newSize,
                                             D3D12_RESOURCE_FLAG_NONE,
                                             D3D12_RESOURCE_STATE_COPY_SOURCE);
    m_gpuBuffer = d3d()->makeBuffer(newSize,
                                    m_bindFlags,
                                    D3D12_RESOURCE_STATE_COMMON,
                                    m_heapFlags);
}

rcp<D3D12Buffer> D3D12ResourceManager::makeBuffer(
    UINT size,
    D3D12_RESOURCE_FLAGS bindFlags,
    D3D12_RESOURCE_STATES initialState,
    D3D12_HEAP_FLAGS heapFlags)
{
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    return make_rcp<D3D12Buffer>(ref_rcp(this),
                                 m_device.Get(),
                                 initialState,
                                 heapFlags,
                                 size,
                                 desc);
}

rcp<D3D12Buffer> D3D12ResourceManager::makeUploadBuffer(
    UINT size,
    D3D12_RESOURCE_FLAGS bindFlags,
    D3D12_RESOURCE_STATES initialState,
    D3D12_HEAP_FLAGS heapFlags)
{
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);

    return make_rcp<D3D12Buffer>(
        ref_rcp(this),
        m_device.Get(),
        initialState,
        heapFlags,
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        size,
        desc);
}

rcp<D3D12VolatileBuffer> D3D12ResourceManager::makeVolatileBuffer(
    UINT size,
    D3D12_RESOURCE_FLAGS bindFlags,
    D3D12_HEAP_FLAGS heapFlags)
{
    return make_rcp<D3D12VolatileBuffer>(ref_rcp(this),
                                         size,
                                         bindFlags,
                                         heapFlags);
}

rcp<D3D12DescriptorHeap> D3D12ResourceManager::makeHeap(
    UINT numDescriptors,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    return make_rcp<D3D12DescriptorHeap>(ref_rcp(this),
                                         m_device.Get(),
                                         numDescriptors,
                                         type,
                                         flags);
}

rcp<D3D12TextureArray> D3D12ResourceManager::make1DTextureArray(
    UINT width,
    UINT16 length,
    UINT mipLevelCount,
    DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS bindFlags,
    D3D12_RESOURCE_STATES initialState,
    D3D12_HEAP_FLAGS heapFlags)
{
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex1D(format,
                                                            width,
                                                            length,
                                                            mipLevelCount,
                                                            bindFlags);
    return make_rcp<D3D12TextureArray>(ref_rcp(this),
                                       m_device.Get(),
                                       initialState,
                                       heapFlags,
                                       desc);
}

rcp<D3D12Texture> D3D12ResourceManager::make2DTexture(
    UINT width,
    UINT height,
    UINT mipLevelCount,
    DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS bindFlags,
    D3D12_RESOURCE_STATES initialState,
    D3D12_HEAP_FLAGS heapFlags,
    const D3D12_CLEAR_VALUE* clearValue)
{
    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format,
                                                            width,
                                                            height,
                                                            1,
                                                            mipLevelCount,
                                                            1,
                                                            0,
                                                            bindFlags);

    return make_rcp<D3D12Texture>(ref_rcp(this),
                                  m_device.Get(),
                                  initialState,
                                  heapFlags,
                                  desc,
                                  clearValue);
}

rcp<D3D12Texture> D3D12ResourceManager::makeExternalTexture(
    ComPtr<ID3D12Resource> externalTexture,
    D3D12_RESOURCE_STATES lastState)
{
    return make_rcp<D3D12Texture>(ref_rcp(this), externalTexture, lastState);
}

void D3D12ResourceManager::transition(ID3D12GraphicsCommandList* cmdList,
                                      D3D12Resource* resource,
                                      D3D12_RESOURCE_STATES toState)
{
    assert(resource->m_lastState != toState);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource->resource(),
                                                        resource->m_lastState,
                                                        toState);
    cmdList->ResourceBarrier(1, &barrier);
    resource->m_lastState = toState;
}

void D3D12ResourceManager::clearUAV(ID3D12GraphicsCommandList* cmdList,
                                    ID3D12Resource* resource,
                                    CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle,
                                    CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
                                    const UINT clearColor[4],
                                    bool needsBarrier)
{
    if (needsBarrier)
    {
        D3D12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::UAV(resource)};

        cmdList->ResourceBarrier(1, barriers);
    }

    cmdList->ClearUnorderedAccessViewUint(gpuHandle,
                                          cpuHandle,
                                          resource,
                                          clearColor,
                                          0,
                                          nullptr);

    if (needsBarrier)
    {
        D3D12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::UAV(resource)};

        cmdList->ResourceBarrier(1, barriers);
    }
}

void D3D12ResourceManager::clearUAV(ID3D12GraphicsCommandList* cmdList,
                                    ID3D12Resource* resource,
                                    CD3DX12_GPU_DESCRIPTOR_HANDLE& gpuHandle,
                                    CD3DX12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
                                    const float clearColor[4],
                                    bool needsBarrier)
{
    if (needsBarrier)
    {
        D3D12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::UAV(resource)};

        cmdList->ResourceBarrier(1, barriers);
    }

    cmdList->ClearUnorderedAccessViewFloat(gpuHandle,
                                           cpuHandle,
                                           resource,
                                           clearColor,
                                           0,
                                           nullptr);

    if (needsBarrier)
    {
        D3D12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::UAV(resource)};

        cmdList->ResourceBarrier(1, barriers);
    }
}

D3D12VolatileBufferPool::D3D12VolatileBufferPool(
    rcp<D3D12ResourceManager> manager,
    UINT alignment,
    UINT size) :
    GPUResourcePool(std::move(manager), MAX_POOL_SIZE),
    m_targetSize(std::max<UINT>(size, m_alignment)),
    m_alignment(alignment)
{}

inline D3D12ResourceManager* D3D12VolatileBufferPool::d3d() const
{
    return static_cast<D3D12ResourceManager*>(m_manager.get());
}

void D3D12VolatileBufferPool::setTargetSize(size_t size)
{
    m_targetSize = std::max<UINT>(static_cast<UINT>(size), m_alignment);
    assert(m_targetSize % m_alignment == 0);
}

rcp<D3D12VolatileBuffer> D3D12VolatileBufferPool::acquire()
{
    auto buffer =
        static_rcp_cast<D3D12VolatileBuffer>(GPUResourcePool::acquire());
    if (buffer == nullptr)
    {
        buffer = d3d()->makeVolatileBuffer(m_targetSize);
    }
    else if (buffer->sizeInBytes() != m_targetSize)
    {
        buffer->resizeBuffers(m_targetSize);
    }
    return buffer;
}

D3D12DescriptorHeapPool::D3D12DescriptorHeapPool(
    rcp<D3D12ResourceManager> manager,
    UINT numDescriptors,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags) :
    GPUResourcePool(std::move(manager), MAX_POOL_SIZE),
    m_numDescriptors(numDescriptors),
    m_type(type),
    m_flags(flags)
{}

rcp<D3D12DescriptorHeap> D3D12DescriptorHeapPool::acquire()
{
    auto heap =
        static_rcp_cast<D3D12DescriptorHeap>(GPUResourcePool::acquire());
    if (heap == nullptr)
    {
        heap = d3d()->makeHeap(m_numDescriptors, m_type, m_flags);
    }

    return heap;
}

inline D3D12ResourceManager* D3D12DescriptorHeapPool::d3d() const
{
    return static_cast<D3D12ResourceManager*>(m_manager.get());
}

}; // namespace rive::gpu