#pragma once
#include <d3d12.h>
#include "Engine/Engine.h"

class RenderPipeline
{
public:
	RenderPipeline() = default;	// Constructor
	~RenderPipeline() = default;	// Destructor

	void Initialize(ID3D12Device* pDevice) {};	// Initialization

private:
	ID3D12Device* m_pDevice = nullptr;	// Device

};