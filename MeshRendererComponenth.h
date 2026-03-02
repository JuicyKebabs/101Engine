#pragma once
#include "Component.h"
#include <memory>

class MeshGPU;

// MeshRendererComponent Class
class MeshRendererComponent : public Component
{
public:
	MeshRendererComponent(Actor* owner, MeshGPU* mesh) : Component(owner), m_pMeshHandle(mesh) {}
	~MeshRendererComponent() = default;

	MeshGPU* GetMeshHandle() const { return m_pMeshHandle.get(); }	// Get mesh handle

private:
	std::unique_ptr<MeshGPU> m_pMeshHandle;
};
