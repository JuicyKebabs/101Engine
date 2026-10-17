#include "TextureGpuData.h"
#include "ApiResult.h"

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace rendering::backend
{
    
Result TextureGpuData::InitializeSrv(ID3D12Device* device)
{
    if (!device || !texture || m_srvHeap.IsInitialized())
    {
        return { false, "Texture SRV requires a device, texture and empty storage." };
    }

    const auto resource = texture->GetDesc();

    if (resource.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        resource.DepthOrArraySize != 1 || resource.SampleDesc.Count != 1 ||
        (resource.Format != DXGI_FORMAT_R8G8B8A8_UNORM && resource.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) ||
        (resource.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) != 0)
    {
        return { false, "Unsupported texture SRV resource." };
    }

    Result result = m_srvHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);

    if (!result.IsSuccess())
    {
        return result;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle{};

    result = m_srvHeap.GetCpuHandle(0, handle);

    if (!result.IsSuccess())
    {
        m_srvHeap.Finalize();
        return result;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = resource.Format;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MostDetailedMip = 0;
    view.Texture2D.MipLevels = resource.MipLevels;
    view.Texture2D.PlaneSlice = 0;
    view.Texture2D.ResourceMinLODClamp = 0.0f;
    device->CreateShaderResourceView(texture.Get(), &view, handle);

    // CreateShaderResourceView has no HRESULT; arguments are constrained above.
    const HRESULT removed = device->GetDeviceRemovedReason();

    if (FAILED(removed))
    {
        m_srvHeap.Finalize();
        return MakeApiFailure("CreateShaderResourceView(Texture): device removed", removed);
    }

    m_srv = handle;
    return {};
}

Result PrepareTextureUpload(
    ID3D12Device* device,
    const detail::TextureCpuData& cpu,
    std::unique_ptr<TextureGpuData>& gpuOutput,
    std::unique_ptr<TextureUploadData>& uploadOutput)
{
    if (!device || gpuOutput || uploadOutput)
    {
        return { false, "Texture upload requires a device and empty outputs." };
    }

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    switch (cpu.format)
    {
    case TextureFormat::RGBA8_UNorm:
        format = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;

    case TextureFormat::RGBA8_sRGB:
        format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        break;

    default:
        return {false, "Unsupported texture upload format."};
    }

    if (cpu.width == 0 || cpu.height == 0 ||
        cpu.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        cpu.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        cpu.mips.empty() ||
        cpu.mips.size() > std::bit_width(std::max(cpu.width, cpu.height)))
    {
        return { false, "Texture upload dimensions or mip count are unsupported." };
    }

    try
    {
        auto gpu = std::make_unique<TextureGpuData>();
        auto upload = std::make_unique<TextureUploadData>();

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = cpu.width;
        desc.Height = cpu.height;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = static_cast<UINT16>(cpu.mips.size());
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;

        HRESULT hr = device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&gpu->texture));

        if (FAILED(hr))
        {
            return MakeApiFailure("CreateCommittedResource(Texture)", hr);
        }

        const auto count = static_cast<UINT>(cpu.mips.size());
        upload->layouts.resize(count);

        std::vector<UINT> rows(count);
        std::vector<UINT64> rowBytes(count);
        UINT64 totalBytes = 0;

        device->GetCopyableFootprints(
            &desc, 0, count, 0, upload->layouts.data(),
            rows.data(), rowBytes.data(), &totalBytes);

        constexpr auto maxSize = (std::numeric_limits<std::size_t>::max)();

        if (totalBytes == 0 ||
            totalBytes == (std::numeric_limits<UINT64>::max)() ||
            totalBytes > maxSize)
        {
            return { false, "Texture upload footprint size is invalid." };
        }

        std::uint32_t width = cpu.width;
        std::uint32_t height = cpu.height;

        for (UINT level = 0; level < count; ++level)
        {
            const auto& source = cpu.mips[level];
            const auto& layout = upload->layouts[level];
            const auto bytes = static_cast<std::size_t>(width) * 4;

            if (source.rowPitch < bytes ||
                height - 1 >(maxSize - bytes) / source.rowPitch ||
                source.pixels.size() < (height - 1) * source.rowPitch + bytes)
            {
                return { false, "Texture CPU mip range is invalid." };
            }

            if (layout.Footprint.Width != width || layout.Footprint.Height != height ||
                layout.Footprint.Depth != 1 || rows[level] != height ||
                rowBytes[level] != bytes || layout.Footprint.RowPitch < bytes ||
                layout.Offset > totalBytes || bytes > totalBytes - layout.Offset ||
                height - 1 >
                (totalBytes - layout.Offset - bytes) / layout.Footprint.RowPitch)
            {
                return {false, "Texture upload footprint range is invalid."};
            }

            width = std::max(1u, width / 2);
            height = std::max(1u, height / 2);
        }

        Result result = upload->buffer.Initialize(device, totalBytes, BufferMemory::Upload);

        if (!result.IsSuccess())
        {
            return result;
        }

        // Write directly to the new Upload buffer: no extra packed CPU copy.
        void* mapped = nullptr;
        const D3D12_RANGE noRead{0, 0};
        auto* resource = upload->buffer.GetNativeResource();

        hr = resource->Map(0, &noRead, &mapped);

        if (FAILED(hr))
        {
            return MakeApiFailure("Map(Texture Upload)", hr);
        }

        auto* destination = static_cast<std::byte*>(mapped);

        for (UINT level = 0; level < count; ++level)
        {
            const auto& source = cpu.mips[level];
            const auto& layout = upload->layouts[level];

            for (UINT row = 0; row < rows[level]; ++row)
            {
                std::memcpy(
                destination + static_cast<std::size_t>(layout.Offset) + static_cast<std::size_t>(row) * layout.Footprint.RowPitch,
                source.pixels.data() + static_cast<std::size_t>(row) * source.rowPitch,
                static_cast<std::size_t>(rowBytes[level]));
            }
        }

        const D3D12_RANGE written{0, static_cast<SIZE_T>(totalBytes)};
        resource->Unmap(0, &written);

        result = gpu->InitializeSrv(device);

        if (!result.IsSuccess())
        {
            return result;
        }

        gpuOutput = std::move(gpu);
        uploadOutput = std::move(upload);

        return {};
    }
    catch (const std::bad_alloc&)
    {
        return {false, "Texture upload allocation failed."};
    }
    catch (const std::length_error&)
    {
        return {false, "Texture upload array size is unsupported."};
    }
}

Result RecordTextureUpload(
    CommandContext& context,
    const TextureGpuData& gpu,
    const TextureUploadData& upload)
{
    if (!context.IsRecording() || context.GetNativeCommandList()->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
        return { false, "Texture upload requires a recording DIRECT context." };
    }

    if (!gpu.texture || !gpu.GetSrvHandle().ptr || !upload.buffer.GetNativeResource() ||
        upload.layouts.size() != gpu.texture->GetDesc().MipLevels)
    {
        return { false, "Texture upload resources or mip count are inconsistent." };
    }

    for (UINT mip = 0; mip < upload.layouts.size(); ++mip)
    {
        const Result result = context.CopyTextureRegion(
            gpu.texture.Get(), 
            mip, upload.buffer.GetNativeResource(), 
            upload.layouts[mip]);

        if (!result.IsSuccess())
        {
            return result;
        }
    }

    // This only records the planned state. Ready is set after Execute + Signal.
    return context.TransitionResource(
        gpu.texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

}

