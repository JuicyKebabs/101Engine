#include "EditorRenderAssets.h"
#include "Engine/Actor/ActorHierarchyActivity.h"
#include "EditorShaderLoader.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Component/Collider.h"

#include <cstring>
#include <limits>
#include <unordered_map>

namespace
{
rendering::Result Failure(const std::string& message)
{
	rendering::Result result;
	result.succeeded = false;
	result.message = "EditorRenderAssets: " + message;
	return result;
}

void AddParameter(rendering::MaterialDesc& material, const char* name, rendering::MaterialValue value)
{
	rendering::MaterialParameter parameter;
	parameter.name = name;
	parameter.value = std::move(value);
	material.parameters.push_back(std::move(parameter));
}
}

EditorRenderAssets::~EditorRenderAssets()
{
	// Order is logical dependencies first. Each request is independent from
	// physical GPU release, so a still-executing previous frame stays valid.

	for (const auto handle : m_pickingMaterials)
	{
		if (handle != rendering::MaterialHandle{})
		{
			(void)m_renderer.DestroyMaterial(handle);
		}
	}

	for (const auto handle : m_maskMaterials)
	{
		const auto result = m_renderer.DestroyMaterial(handle);
		(void)result;
	}

	for (const auto handle : {m_outlineMaterial, m_colliderMaterial})
	{
		if (handle != rendering::MaterialHandle{})
		{
			const auto result = m_renderer.DestroyMaterial(handle);
			(void)result;
		}
	}

	for (const auto handle : m_debugGeometry)
	{
		if (handle != rendering::GeometryHandle{})
		{
			const auto result = m_renderer.DestroyGeometry(handle);
			(void)result;
		}
	}

	for (const auto handle : m_pipelines)
	{
		if (handle != rendering::PipelineHandle{})
		{
			const auto result = m_renderer.DestroyPipeline(handle);
			(void)result;
		}
	}

	if (m_skinnedMaskPipeline != rendering::PipelineHandle{})
	{
		const auto result = m_renderer.DestroyPipeline(m_skinnedMaskPipeline);
		(void)result;
	}

	for (const auto handle : m_shaders)
	{
		if (handle != rendering::ShaderHandle{})
		{
			const auto result = m_renderer.DestroyShader(handle);
			(void)result;
		}
	}

	for (const auto handle : m_pickingPipelines)
	{
		if (handle != rendering::PipelineHandle{})
		{
			(void)m_renderer.DestroyPipeline(handle);
		}
	}

	for (const auto handle : m_pickingShaders)
	{
		if (handle != rendering::ShaderHandle{})
		{
			(void)m_renderer.DestroyShader(handle);
		}
	}

	for (const auto handle : {m_sceneColor, m_selectionMask})
	{
		if (handle != rendering::RenderTargetTextureHandle{})
		{
			const auto result = m_renderer.DestroyRenderTargetTexture(handle);
			(void)result;
		}
	}
}

rendering::Result EditorRenderAssets::RegisterPipeline(
	std::size_t index,
	std::size_t vertexShader,
	std::size_t pixelShader,
	rendering::BlendMode blend,
	rendering::DepthMode depth,
	rendering::FillMode fill)
{
	rendering::PipelineDesc description;
	description.vertexShader = m_shaders[vertexShader];
	description.pixelShader = m_shaders[pixelShader];
	description.blend = blend;
	description.depth = depth;
	description.cull = rendering::CullMode::None;
	description.fill = fill;
	const auto registration = m_renderer.RegisterPipeline(description);

	if (registration.result.IsSuccess())
	{
		m_pipelines[index] = registration.handle;
	}

	return registration.result;
}

rendering::Result EditorRenderAssets::Initialize(
	const std::filesystem::path& shaderDirectory,
	rendering::ShaderHandle skinnedVertex)
{
	//---------------------------------------------------------------------------------
	// Register vertex shaders (Renderer built-in) for the editor's auxiliary passes.
	//---------------------------------------------------------------------------------
	const std::size_t vsCount = 2;
	const std::array<const char*, vsCount> vertexShaders{"MeshVS.cso", "FullscreenVS.cso"};

	for (std::size_t index = 0; index < vertexShaders.size(); ++index)
	{
		auto registered = LoadAndRegisterShader(m_renderer, shaderDirectory, vertexShaders[index], rendering::ShaderStage::Vertex);
		
		if (!registered.result.IsSuccess())
		{
			return registered.result;
		}

		m_shaders[index] = registered.handle;
	}

	//---------------------------------------------------------------------------------
	// Register original pixel shaders for the editor's auxiliary passes.
	//---------------------------------------------------------------------------------
	std::string solidMask = "SolidMask.cso";
	std::string alphaMask = "AlphaMask.cso";
	std::string outline = "Outline.cso";
	std::string collider = "Collider.cso";

	const std::array<std::string, 4> pixelShaders{solidMask, alphaMask, outline, collider};
	
	for (std::size_t index = 0; index < pixelShaders.size(); ++index)
	{
		auto registered = LoadAndRegisterShader(m_renderer, shaderDirectory, pixelShaders[index], rendering::ShaderStage::Pixel);

		if (!registered.result.IsSuccess())
		{
			return registered.result;
		}

		m_shaders[index + vsCount] = registered.handle;
	}

	//---------------------------------------------------------------------------------
	// Register pipelines for the editor's auxiliary passes.
	//---------------------------------------------------------------------------------
	for (std::size_t index = 0; index < m_pipelines.size(); ++index)
	{
		const bool isOutline = index == 2;
		const bool isCollider = index == 3;
		auto registered = RegisterPipeline(
			index,
			isOutline ? 1 : 0,
			index + 2,
			isOutline ? rendering::BlendMode::Alpha : rendering::BlendMode::Opaque,
			isCollider ? rendering::DepthMode::ReadOnly : rendering::DepthMode::Disabled,
			isCollider ? rendering::FillMode::Wireframe : rendering::FillMode::Solid);

		if (!registered.IsSuccess())
		{
			return registered;
		}
	}

	//---------------------------------------------------------------------------------
	// Register a pipeline for selection mask rendering of skinned meshes.
	// This is a special case because the vertex shader is provided externally.
	//---------------------------------------------------------------------------------
	rendering::PipelineDesc skinnedMask;
	skinnedMask.vertexShader = skinnedVertex;
	skinnedMask.pixelShader = m_shaders[vsCount];
	skinnedMask.depth = rendering::DepthMode::Disabled;
	skinnedMask.cull = rendering::CullMode::None;
	const auto skinnedMaskRegistration = m_renderer.RegisterPipeline(skinnedMask);
	
	if (!skinnedMaskRegistration.result.IsSuccess())
	{
		return skinnedMaskRegistration.result;
	}
	
	m_skinnedMaskPipeline = skinnedMaskRegistration.handle;

	//---------------------------------------------------------------------------------
	// Create render target textures for scene color.
	//---------------------------------------------------------------------------------
	rendering::RenderTargetTextureDesc target;
	target.width = m_width;
	target.height = m_height;
	target.format = rendering::TextureFormat::RGBA8_sRGB;
	auto color = m_renderer.CreateRenderTargetTexture(target);

	if (!color.result.IsSuccess())
	{
		return color.result;
	}

	m_sceneColor = color.handle;

	//---------------------------------------------------------------------------------
	// Create render target textures for selection mask.
	//---------------------------------------------------------------------------------
	target.format = rendering::TextureFormat::RGBA8_UNorm;

	// Numeric coverage: Alpha zero means unselected. Match allocation advice
	// to the actual auxiliary Clear without changing the Outline algorithm.
	target.optimizedClearColor.fill(0.0f);
	auto mask = m_renderer.CreateRenderTargetTexture(target);

	if (!mask.result.IsSuccess())
	{
		return mask.result;
	}

	m_selectionMask = mask.handle;

	//---------------------------------------------------------------------------------
	// Create render target textures for picking Actor IDs.
	//---------------------------------------------------------------------------------
	
	target.format = rendering::TextureFormat::R32_UInt;
	const auto picking = m_renderer.CreateRenderTargetTexture(target);

	if (!picking.result.IsSuccess())
	{
		return picking.result;
	}

	m_pickingTarget = picking.handle;

	// Register shaders for picking Actor IDs.
	const std::array<const char*, 3> pickingFiles{ "PickingVS.cso", "PickingSolidPS.cso", "PickingTexturePS.cso" };
	
	for (std::size_t index = 0; index < pickingFiles.size(); ++index)
	{
		const auto stage = index == 0 ? rendering::ShaderStage::Vertex : rendering::ShaderStage::Pixel;

		auto registered = LoadAndRegisterShader(m_renderer, shaderDirectory, pickingFiles[index], stage);

		if (!registered.result.IsSuccess())
		{
			return registered.result;
		}

		m_pickingShaders[index] = registered.handle;
	}

	//---------------------------------------------------------------------------------
	// Register pipelines for picking Actor IDs.
	//---------------------------------------------------------------------------------
	
	// Standard geometry, textured quad, skinned geometry.
	// All built-in inputs are shared with the editor's auxiliary passes.
	for (std::size_t index = 0; index < m_pickingPipelines.size(); ++index)
	{
		rendering::PipelineDesc description;
		description.vertexShader = index == 2 ? skinnedVertex : m_pickingShaders[0];
		description.pixelShader = m_pickingShaders[index == 1 ? 2 : 1];
		description.cull = rendering::CullMode::None;
		description.depth = rendering::DepthMode::ReadOnly;
		
		const auto registered = m_renderer.RegisterPipeline(description);

		if (!registered.result.IsSuccess())
		{
			return registered.result;
		}

		m_pickingPipelines[index] = registered.handle;
	}


	//---------------------------------------------------------------------------------
	// Create materials for selection outline visualization.
	//---------------------------------------------------------------------------------
	rendering::MaterialDesc outlineDescription;
	outlineDescription.pipeline = m_pipelines[2];
	AddParameter(outlineDescription, "SelectionMask", m_selectionMask);
	const auto outlineRegistration = m_renderer.RegisterMaterial(outlineDescription);

	if (!outlineRegistration.result.IsSuccess())
	{
		return outlineRegistration.result;
	}

	m_outlineMaterial = outlineRegistration.handle;

	//---------------------------------------------------------------------------------
	// Create materials for collider visualization.
	//---------------------------------------------------------------------------------
	rendering::MaterialDesc colliderDescription;
	colliderDescription.pipeline = m_pipelines[3];
	Vector4 identity;
	identity.x = 1;
	identity.y = 1;
	AddParameter(colliderDescription, "LocalQuadTransform", identity);
	const auto colliderRegistration = m_renderer.RegisterMaterial(colliderDescription);

	if (!colliderRegistration.result.IsSuccess())
	{
		return colliderRegistration.result;
	}

	m_colliderMaterial = colliderRegistration.handle;

	//---------------------------------------------------------------------------------
	// Create debug geometry for colliders.
	//---------------------------------------------------------------------------------
	const std::array<DefaultMesh, 3> types{DefaultMesh::Cube, DefaultMesh::Sphere, DefaultMesh::Cylinder};

	for (std::size_t index = 0; index < types.size(); ++index)
	{
		// Get the default model for the current mesh type.
		const Model model = GetDefaultModel(types[index]);
		const Mesh& mesh = model.at(0);

		// Convert the mesh vertices to the rendering engine's vertex format.
		std::vector<rendering::MeshVertex> vertices;
		vertices.reserve(mesh.vertices.size());

		for (const Vertex& source : mesh.vertices)
		{
			rendering::MeshVertex vertex;
			vertex.position = source.position;
			vertex.normal = source.normal;
			vertex.uv.x = source.uv.x;
			vertex.uv.y = source.uv.y;
			vertex.tangent = source.tangent;
			vertex.color = source.color;
			vertices.push_back(vertex);
		}

		// Register the geometry with the renderer and store the handle for later use.
		rendering::GeometryDesc description;
		description.vertices = vertices;
		description.indices = mesh.indices;
		const auto registered = m_renderer.RegisterGeometry(description);

		if (!registered.result.IsSuccess())
		{
			return registered.result;
		}

		m_debugGeometry[index] = registered.handle;
	}

	return {};
}

rendering::Result EditorRenderAssets::Resize(std::uint32_t width, std::uint32_t height)
{
	// Each Renderer resize replaces its own complete physical resource set.
	// failure to stop this frame and exit; it does not retry or draw the pair.
	// Each target commits independently. Publish dimensions only after all succeed.
	// The host stops rendering on failure rather than drawing mismatched extents.
	auto color = m_renderer.ResizeRenderTargetTexture(m_sceneColor, width, height);

	if (!color.IsSuccess())
	{
		return color;
	}

	auto mask = m_renderer.ResizeRenderTargetTexture(m_selectionMask, width, height);

	if (!mask.IsSuccess())
	{
		return mask;
	}

	auto picking = m_renderer.ResizeRenderTargetTexture(m_pickingTarget, width, height);

	if (!picking.IsSuccess())
	{
		return picking;
	}

	m_width = width;
	m_height = height;
	return {};
}

rendering::Result EditorRenderAssets::AppendMaskMaterial(
	const rendering::MaterialDesc& source,
	bool alphaMask,
	bool  skinned,
	std::size_t slot,
	rendering::MaterialHandle& output)
{
	// Reserve the owning Handle slot before registration so allocation failure
	// cannot strand a successfully created Material registration.
	if (slot >= m_maskMaterials.size())
	{
		m_maskMaterials.resize(slot + 1);
	}

	rendering::MaterialDesc description = source;
	description.pipeline = m_pipelines[alphaMask ? 1 : 0];
	description.shadowPipeline.reset();

	// Override the pipeline for skinned meshes,
	// which use a different vertex shader for skinning.
	if (skinned)
	{
		description.pipeline = m_skinnedMaskPipeline;
	}

	if (!alphaMask)
	{
		std::erase_if(
			description.parameters,
			[](const rendering::MaterialParameter& parameter)
			{
				return parameter.name == "BaseTexture";
			});
	}

	output = m_maskMaterials[slot];

	if (output != rendering::MaterialHandle{})
	{
		return m_renderer.UpdateMaterial(output, description);
	}

	const auto registered = m_renderer.RegisterMaterial(description);

	if (registered.result.IsSuccess())
	{
		m_maskMaterials[slot] = registered.handle;
		output = registered.handle;
	}

	return registered.result;
}

rendering::Result EditorRenderAssets::AppendSelection(
	rendering::FrameRenderData& frame,
	SceneBase& scene,
	const Guid& selectedActor,
	rendering::ViewId sourceView,
	bool canvasPreview)
{
	if (!selectedActor.IsValid())
	{
		return {};
	}

	//-------------------------------------------------------------
	// Create an auxiliary pass for the selection mask.
	//-------------------------------------------------------------
	rendering::AuxiliaryPass mask;
	mask.id = 10;
	mask.sourceViewId = sourceView;
	mask.name = "Editor.SelectionMask";
	mask.stage = canvasPreview ? rendering::AuxiliaryStage::AfterOverlay : rendering::AuxiliaryStage::BeforeDisplayTransform;
	mask.colorOutput = m_selectionMask;
	mask.colorLoad = rendering::AuxiliaryColorLoad::Clear;
	// Existing selection behavior includes occluded objects. No Scene Depth.
	mask.useSourceDepth = false;

	std::size_t materialSlot = 0;

	// Find selected Actor in the extracted render objects and
	// change the it's Material to a mask Material for the auxiliary pass.
	for (const auto& record : scene.GetRenderSystem()->GetExtractedObjects())
	{
		if (record.actorGuid != selectedActor || record.viewId != sourceView || !record.material)
		{
			continue;
		}

		rendering::MaterialHandle maskMaterial;

		// Create a mask Material for this object, using the original Material's description.
		// (because their is no MaterialInstance concept in the Renderer....)
		auto prepared = AppendMaskMaterial(
			record.material->description,
			record.alphaMask,
			record.skinned,
			materialSlot++,
			maskMaterial);

		if (!prepared.IsSuccess())
		{
			return prepared;
		}

		rendering::AuxiliaryDraw draw;
		draw.objectIndex = record.objectIndex;	// Do not duplicate the object; just reference it in the auxiliary pass.
		draw.materialOverride = maskMaterial;	// do not change the original Material; just override it for this auxiliary pass.
		mask.draws.push_back(draw);
	}

	if (mask.draws.empty())
	{
		return {};
	}

	// Add fullscreen object to the frame render data for the mask auxiliary pass.
	// This is used to render the selection mask to the selection mask render target.
	rendering::RenderObject fullscreen;
	fullscreen.material = m_outlineMaterial;
	fullscreen.targetViews.push_back(11);

	// Fullscreen triangle geometry is a procedural draw; no vertex/index buffers are needed.
	rendering::ProceduralGeometry triangle;
	triangle.vertexCount = 3;
	fullscreen.procedural = triangle;

	frame.objects.push_back(fullscreen);

	//-------------------------------------------------------------
	// Create an auxiliary pass for the outline effect.
	//-------------------------------------------------------------
	rendering::AuxiliaryPass outline;
	outline.id = 11;
	outline.sourceViewId = sourceView;
	outline.name = "Editor.SelectionOutline";
	outline.stage = mask.stage;

	const std::size_t outlineObject = frame.objects.size() -1;	// Next object index.

	rendering::AuxiliaryDraw draw;
	draw.objectIndex = outlineObject;
	outline.draws.push_back(draw);

	// Add the auxiliary passes to the frame render data.
	frame.auxiliaryPasses.push_back(std::move(mask));
	frame.auxiliaryPasses.push_back(std::move(outline));

	return {};
}

rendering::Result EditorRenderAssets::AppendPicking(
	rendering::FrameRenderData& frame,
	std::span<const ExtractedObjectRecord> objects,
	rendering::ViewId sourceView,
	bool canvasPreview,
	std::vector<Guid>& actorIds)
{
	rendering::AuxiliaryPass pass;
	pass.id = PickingPassId;
	pass.sourceViewId = sourceView;
	pass.name = "Editor.Picking";

	// The timing of picking pass is depend on the kind of view.
	pass.stage = 
		canvasPreview ? rendering::AuxiliaryStage::AfterOverlay
		: rendering::AuxiliaryStage::BeforeDisplayTransform;

	pass.colorOutput = m_pickingTarget;
	pass.colorLoad = rendering::AuxiliaryColorLoad::Clear;

	// Scene view uses the depth based ordering for picking.
	pass.useSourceDepth = !canvasPreview;

	std::vector<Guid> ids(1);	// Actor IDs which have already been recorded with picking IDs.

	std::unordered_map<Guid, std::uint32_t> actorIdTable;	// Maps Actor GUIDs to their picking ID (index in the ids vector).

	for (const auto& record : objects)
	{
		if (record.viewId != sourceView)
		{
			continue;
		}

		if (!record.material || !record.actorGuid.IsValid())
		{
			return Failure("AppendPicking: invalid material or actor GUID.");
		}

		if (ids.size() > (std::numeric_limits<std::uint32_t>::max)())
		{
			return Failure("AppendPicking: too many actors for picking.");
		}

		// Map Actor GUID to a picking ID (index in the ids vector).
		// Picking ID is consecutive and starts at 1. Index 0 is reserved for "no actor".
		const auto [entry, inserted] = actorIdTable.try_emplace(record.actorGuid, static_cast<std::uint32_t>(ids.size()));

		if (inserted)
		{
			// New Actor GUID, add it to the list of Actor IDs for this picking pass.
			ids.push_back(record.actorGuid);
		}

		// Skinned meshs requires the pipeline with the skinned vertex shader.
		// (magic number 2 is the index of the skinned pipeline in m_pickingPipelines.)
		const std::size_t pipelineIndex = record.skinned ? 2 : (record.alphaMask ? 1 : 0);

		// Create a new Material for this object, using the original Material's description.
		// Currently, before implementing MaterialInstance, we have to create a new material for every picking ID.
		rendering::MaterialDesc material;
		material.pipeline = m_pickingPipelines[pipelineIndex];
		material.parameters = record.material->description.parameters;
		AddParameter(material, "SelectionId", entry->second);

		// Reserve a slot in the picking materials vector for this object.
		const auto slot = pass.draws.size();

		if (slot == m_pickingMaterials.size())
		{
			m_pickingMaterials.emplace_back();
		}

		// Register the new Material with the Renderer 
		// and store the handle in m_pickingMaterials.
		auto& handle = m_pickingMaterials[slot];

		if (handle == rendering::MaterialHandle{})
		{
			// Register a new Material with the Renderer.
			const auto registered = m_renderer.RegisterMaterial(material);

			if (!registered.result.IsSuccess())
			{
				return registered.result;
			}

			handle = registered.handle;
		}
		else
		{
			// Update the existing Material with the new description.
			const auto updated = m_renderer.UpdateMaterial(handle, material);

			if (!updated.IsSuccess())
			{
				return updated;
			}
		}

		// Add a draw command to the picking auxiliary pass for this object
		// with the picking Material that was just created or updated.
		rendering::AuxiliaryDraw draw;
		draw.objectIndex = record.objectIndex;
		draw.materialOverride = handle;
		pass.draws.push_back(draw);
	}

	// Even if there is no object to draw, 
	// it is needed to clear the picking render target
	// to zero for picking "no actor" in the next frame.
	frame.auxiliaryPasses.push_back(std::move(pass));
	actorIds = std::move(ids);

	return {};
}

bool EditorRenderAssets::ResolvePickingPixel(
	const rendering::TextureReadbackData& data,
	std::span<const Guid> actorIds,
	Guid& actor)
{
	// Picking single R32_UINT pixel in tightly packed format is expected.
	const bool isPixel = data.width == 1 && data.height == 1;
	const bool isUInt = data.format == rendering::TextureFormat::R32_UInt;
	const bool tightlyPacked = 
		data.rowPitch == sizeof(std::uint32_t) &&
		data.pixels.size() == data.rowPitch;

	if (!isPixel || !isUInt || !tightlyPacked)
	{
		return false;
	}

	std::uint32_t id = 0;
	std::memcpy(&id, data.pixels.data(), sizeof(id));

	if (id >= actorIds.size())
	{
		return false;
	}

	actor = actorIds[id];
	return true;
}

void EditorRenderAssets::AppendColliderObject(
	rendering::FrameRenderData& frame,
	rendering::AuxiliaryPass& pass,
	rendering::GeometryHandle geometry,
	const Matrix4x4& world)
{
	rendering::RenderObject object;
	object.geometry = geometry;
	object.material = m_colliderMaterial;
	object.world = world;
	object.targetViews.push_back(pass.id);
	rendering::AuxiliaryDraw draw;
	draw.objectIndex = frame.objects.size();
	frame.objects.push_back(object);
	pass.draws.push_back(draw);
}

void EditorRenderAssets::AppendColliders(
	rendering::FrameRenderData& frame,
	SceneBase& scene,
	rendering::ViewId sourceView)
{
	rendering::AuxiliaryPass pass;
	pass.id = 12;
	pass.sourceViewId = sourceView;
	pass.name = "Editor.ColliderWireframe";
	pass.useSourceDepth = true;

	for (Actor* actor : scene.GetAllActors())
	{
		if (!IsActorHierarchyActive(actor))
		{
			continue;
		}

		for (Collider* collider : actor->GetComponentsByClass<Collider>())
		{
			if (!collider || !collider->isActive() || collider->GetType() == ColliderType::None)
			{
				continue;
			}

			collider->Flush();

			if (collider->GetType() == ColliderType::CAPSULE)
			{
				const CapsuleCollider& capsule = collider->GetCurrentCapsuleCollider();
				const float diameter = capsule.radius * 2;
				const auto transform = collider->GetWorldTransformCurrent();
				const Vector3 sphereScale(diameter, diameter, diameter);

				if (capsule.cylHeight <= 0)
				{
					AppendColliderObject(
						frame,
						pass,
						m_debugGeometry[1],
						Matrix4x4::CreateTRS(
							transform.position,
							Quaternion::Identity(),
							sphereScale));
				}
				else
				{
					const Vector3 cylinderScale(diameter, capsule.cylHeight, diameter);
					AppendColliderObject(
						frame,
						pass,
						m_debugGeometry[2],
						Matrix4x4::CreateTRS(
							transform.position,
							transform.rotation,
							cylinderScale));
					AppendColliderObject(
						frame,
						pass,
						m_debugGeometry[1],
						Matrix4x4::CreateTRS(
							capsule.pointA,
							Quaternion::Identity(),
							sphereScale));
					AppendColliderObject(
						frame,
						pass,
						m_debugGeometry[1],
						Matrix4x4::CreateTRS(
							capsule.pointB,
							Quaternion::Identity(),
							sphereScale));
				}
			}
			else if (collider->GetType() == ColliderType::BOX || collider->GetType() == ColliderType::SPHERE)
			{
				const std::size_t geometryIndex = collider->GetType() == ColliderType::BOX ? 0 : 1;
				AppendColliderObject(
					frame,
					pass,
					m_debugGeometry[geometryIndex],
					collider->GetWorldMatrix());
			}
		}
	}

	if (!pass.draws.empty())
	{
		frame.auxiliaryPasses.push_back(std::move(pass));
	}
}
