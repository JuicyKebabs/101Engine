#include "TextureReadbackTransfer.h"
#include "ApiResult.h"
#include "Backend.h"
#include "RenderTargetTextureGpuData.h"

#include <cstring>
#include <limits>
#include <utility>

namespace rendering::backend
{

namespace
{

bool IsValidSourceRegion(const D3D12_RESOURCE_DESC& source, const PixelRect& region)
{
	const bool isSingleSubresource =
		source.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
		source.DepthOrArraySize == 1 &&
		source.MipLevels == 1;

	if (!isSingleSubresource || source.SampleDesc.Count != 1)
	{
		return false;
	}

	const bool hasValidOrigin = region.x < source.Width && region.y < source.Height;
	const bool isNotEmpty = region.width > 0 && region.height > 0;

	return hasValidOrigin && isNotEmpty &&
		region.width <= source.Width - region.x && region.height <= source.Height - region.y;
}

Result CheckReadbackFailure(const Device& device, Result failure)
{
	const Result deviceStatus = device.CheckStatus();

	if (!deviceStatus.IsSuccess() && deviceStatus.failureKind == FailureKind::DeviceLost)
	{
		failure.failureKind = deviceStatus.failureKind;
		failure.message += "\n";
		failure.message += deviceStatus.message;
	}

	return failure;
}

}	// namespace

Result Backend::PlanTextureReadback(
	const RenderTargetTextureGpuData& source,
	const PixelRect& region,
	TextureReadbackTransfer& transfer) const
{
	auto* device = m_device.GetNativeDevice();
	auto* texture = source.GetNativeResource();

	if (!device || !texture || transfer.m_transferBytes != 0)
	{
		return { false, "PlanTextureReadback: Invalid device, source texture or transfer state." };
	}

	auto regionDescription = texture->GetDesc();

	if (!IsValidSourceRegion(regionDescription, region))
	{
		return { false, "PlanTextureReadback: Invalid source region." };
	}

	regionDescription.Width = region.width;
	regionDescription.Height = region.height;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT64 rowBytes = 0;
	UINT64 transferBytes = 0;

	device->GetCopyableFootprints(
		&regionDescription,
		0,
		1,
		0,
		&footprint,
		nullptr,
		&rowBytes,
		&transferBytes);

	if (transferBytes == 0 ||
		transferBytes == (std::numeric_limits<UINT64>::max)() ||
		rowBytes == 0)
	{
		return { false, "PlanTextureReadback: Invalid footprint or transfer size." };
	}

	const UINT64 paddedBytes = static_cast<UINT64>(footprint.Footprint.RowPitch) * region.height;
	const UINT64 packedBytes = rowBytes * region.height;
	const UINT64 reservedBytes = paddedBytes + packedBytes;

	if (transferBytes > paddedBytes || reservedBytes > (std::numeric_limits<std::size_t>::max)())
	{
		return { false, "PlanTextureReadback: Transfer size exceeds limits." };
	}

	transfer.m_region = region;
	transfer.m_footprint = footprint;
	transfer.m_transferBytes = transferBytes;
	transfer.m_rowBytes = static_cast<std::size_t>(rowBytes);
	transfer.m_reservedBytes = static_cast<std::size_t>(reservedBytes);
	return {};
}

Result Backend::PrepareTextureReadback(TextureReadbackTransfer& transfer)
{
	if (!m_frameActive || m_frameCompletionUnknown || transfer.m_transferBytes == 0)
	{
		return { false, "PrepareTextureReadback requires an active healthy frame and a valid transfer." };
	}

	Result result = transfer.m_buffer.Initialize(
		m_device.GetNativeDevice(),
		transfer.m_transferBytes,
		BufferMemory::Readback);

	if (!result.IsSuccess())
	{
		return CheckReadbackFailure(m_device, std::move(result));
	}

	return {};
}

Result CommandContext::RecordTextureReadback(
	ID3D12Resource* source,
	const TextureReadbackTransfer& transfer)
{
	if (!IsRecording() || !source || !transfer.HasBuffer())
	{
		return { false, "RecordTextureReadback requires an active recording context, a valid source and a prepared transfer." };
	}

	const auto sourceDesc = source->GetDesc();
	const auto& region = transfer.m_region;

	if (!IsValidSourceRegion(sourceDesc, region) ||
		sourceDesc.Format != transfer.m_footprint.Footprint.Format)
	{
		return { false, "RecordTextureReadback: Invalid source region or format mismatch." };
	}

	D3D12_TEXTURE_COPY_LOCATION from{};
	from.pResource = source;
	from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	from.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION to{};
	to.pResource = transfer.m_buffer.GetNativeResource();
	to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	to.PlacedFootprint = transfer.m_footprint;

	D3D12_BOX box{};
	box.left = region.x;
	box.top = region.y;
	box.front = 0;
	box.back = 1;
	box.right = region.x + region.width;
	box.bottom = region.y + region.height;

	m_commandList->CopyTextureRegion(&to, 0, 0, 0, &from, &box);

	return {};
}

Result Backend::ReadTextureReadback(
	TextureReadbackTransfer& transfer,
	std::span<std::byte> destination)
{
	if (!transfer.HasBuffer() || destination.size() != transfer.PackedBytes())
	{
		return { false, "ReadTextureReadback requires a valid transfer and a destination buffer of the correct size." };
	}

	auto* buffer = transfer.m_buffer.GetNativeResource();

	D3D12_RANGE readRange{};
	readRange.End = static_cast<SIZE_T>(transfer.m_transferBytes);

	void* mapped = nullptr;
	const HRESULT mappedResult = buffer->Map(0, &readRange, &mapped);

	if (FAILED(mappedResult))
	{
		return CheckReadbackFailure(m_device, MakeApiFailure("ID3D12Resource::Map(Readback)", mappedResult));
	}

	const auto* source = static_cast<const std::byte*>(mapped) + transfer.m_footprint.Offset;
	const std::size_t sourcePitch = transfer.m_footprint.Footprint.RowPitch;
	const std::size_t rowBytes = transfer.RowBytes();

	for (std::size_t row = 0; row < transfer.m_region.height; ++row)
	{
		std::memcpy(destination.data() + row * rowBytes, source + row * sourcePitch, rowBytes);
	}

	const D3D12_RANGE noWrites{};
	buffer->Unmap(0, &noWrites);

	return {};
}

}
