#include "RenderSystem.h"
#include "Engine/Actor/ActorHierarchyActivity.h"
#include "Engine/Scene/SceneBase.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Component/MeshRenderer.h"
#include "Engine/Component/SkinnedMeshRenderer.h"
#include "Engine/Component/Transform.h"
#include "Engine/Component/SpriteRenderer.h"
#include "Engine/UI/UIRenderer.h"
#include "Engine/UI/Canvas.h"
#include "Engine/Resource/RenderAssetResources.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

template<class ComponentType>
void RegisterOnce(std::vector<ComponentType*>& values, ComponentType* component)
{
	if (component && std::find(values.begin(), values.end(), component) == values.end())
	{
		values.push_back(component);
	}
}

template<class ComponentType>
void UnregisterValue(std::vector<ComponentType*>& values, ComponentType* component)
{
	std::erase(values, component);
}

enum class SourceKind { Mesh, SkinnedMesh, Sprite, UI };

// Temporal structure to track the source of a submitted object for rendering.
struct SubmissionSource
{
	RendererComponent* component = nullptr;	// Pointer to thre source component that submitted the object.
	SourceKind kind = SourceKind::Mesh;		// The kind of the source component (Mesh, SkinnedMesh, Sprite, or UI).
	bool screen = false;					// Whether the submission is for screen space rendering.
	std::vector<std::uint32_t> orderPath;	// The order path for rendering.
};

Vector4 LocalQuadTransform(const ComponentMaterialState& material)
{
	for (const auto& parameter : material.description.parameters)
	{
		if (parameter.name == "LocalQuadTransform")
		{
			if (const auto* value = std::get_if<Vector4>(&parameter.value))
			{
				return *value;
			}
		}
	}

	Vector4 identity;
	identity.x = 1;
	identity.y = 1;
	return identity;
}

// Frobenius norm bounds the maximum stretch, including shear. 
// For Billboard, retain a sphere around the Actor origin so rotating an off-center pivot for
// Camera A/B cannot move Geometry outside a Camera-independent submitted bound.
void SetWorldBounds(
	rendering::RenderObject& object,
	Vector3 localCenter,
	float localRadius,
	std::uint64_t revision,
	const Matrix4x4& world)
{
	// Calculate the stretch factor of the world matrix to scale the local radius
	double stretchSquared = 0;

	for (std::size_t row = 0; row < 3; ++row)
	{
		for (std::size_t column = 0; column < 3; ++column)
		{
			const double value = world.m[row][column];
			stretchSquared += value * value;	// Sum of squares of the 3x3 upper-left submatrix (ignoring translation)
		}
	}

	Vector3 center;
	double radius = localRadius;

	// Considering the billboard mode if needed.
	if (object.billboard == rendering::BillboardMode::None)
	{
		center = world.TransformPoint(localCenter);
	}
	else
	{
		center = world.GetTranslation();
		radius += localCenter.Length();
	}

	radius *= std::sqrt(stretchSquared);

	if (!std::isfinite(radius) || radius > (std::numeric_limits<float>::max)())
	{
		return; // Missing Bounds safely disables culling; do not submit infinity.
	}

	rendering::BoundingSphere bounds;
	bounds.center = center;
	bounds.radius = std::nextafter(static_cast<float>(radius), (std::numeric_limits<float>::infinity)());

	if (!std::isfinite(bounds.radius))
	{
		return;
	}

	object.bounds = bounds;
	object.boundsGeometryRevision = revision;
}

}

void RenderSystem::Register(MeshRenderer* value) { RegisterOnce(m_meshRenderers, value); }
void RenderSystem::Register(SkinnedMeshRenderer* value) { RegisterOnce(m_skinnedMeshRenderers, value); }
void RenderSystem::Register(SpriteRenderer* value) { RegisterOnce(m_spriteRenderers, value); }
void RenderSystem::Register(UIRenderer* value) { RegisterOnce(m_uiRenderers, value); }
void RenderSystem::Unregister(MeshRenderer* value) { UnregisterValue(m_meshRenderers, value); }
void RenderSystem::Unregister(SkinnedMeshRenderer* value) { UnregisterValue(m_skinnedMeshRenderers, value); }
void RenderSystem::Unregister(SpriteRenderer* value) { UnregisterValue(m_spriteRenderers, value); }
void RenderSystem::Unregister(UIRenderer* value) { UnregisterValue(m_uiRenderers, value); }

void RenderSystem::PrepareRegistrationCapacity(std::size_t additionalComponents)
{
	// Preparation may allocate; OnAttach registration in the commit phase must
	// fit these capacities. The count includes all new Components conservatively.
	m_meshRenderers.reserve(m_meshRenderers.size() + additionalComponents);
	m_spriteRenderers.reserve(m_spriteRenderers.size() + additionalComponents);
	m_uiRenderers.reserve(m_uiRenderers.size() + additionalComponents);
}

void RenderSystem::PrepareRegistrationCapacity(const RendererComponent& renderer, std::size_t additionalRenderers)
{
	const auto reserve = [additionalRenderers](auto& registered)
		{
			registered.reserve(registered.size() + additionalRenderers);
		};

	if (dynamic_cast<const MeshRenderer*>(&renderer))
	{
		reserve(m_meshRenderers);
	}
	else if (dynamic_cast<const SkinnedMeshRenderer*>(&renderer))
	{
		reserve(m_skinnedMeshRenderers);
	}
	else if (dynamic_cast<const SpriteRenderer*>(&renderer))
	{
		reserve(m_spriteRenderers);
	}
	else if (dynamic_cast<const UIRenderer*>(&renderer))
	{
		reserve(m_uiRenderers);
	}
}

void RenderSystem::FlushRegisters()
{
	const auto flush = [](const auto& renderers)
	{
		for (auto* renderer : renderers)
		{
			if (!renderer)
			{
				continue;
			}

			Actor* actor = renderer ? renderer->GetOwner() : nullptr;
			const bool live = IsActorHierarchyActive(actor);

			if (live && !renderer->IsDestroyed())
			{
				renderer->Flush();
			}
		}
	};

	flush(m_meshRenderers);
	flush(m_spriteRenderers);
	flush(m_uiRenderers);
}

bool RenderSystem::AppendFrame(
	rendering::FrameRenderData& frame,
	const rendering::ViewOutput& output,
	const CameraInfo* camera,
	const RenderViewPolicy& policy,
	rendering::ViewId sceneViewId,
	rendering::ViewId overlayViewId,
	std::uint32_t width,
	std::uint32_t height,
	const DirectionalLight& light)
{
	if (!m_scene || width == 0 || height == 0)
	{
		return false;
	}

	const EngineContext* context = m_scene->GetEngineContext();

	if (!context)
	{
		return false;
	}

	m_records.clear();
	const bool canvasPreview = policy.canvasPreview || policy.canvasViewRoot;
	Matrix4x4 worldToCanvas = Matrix4x4::Identity();

	if (policy.canvasViewRoot)
	{
		worldToCanvas = policy.canvasViewRoot->GetContentWorldMatrix().Inverse();
	}

	std::vector<SubmissionSource> sources;

	const auto gather = [&](const auto& renderers, SourceKind kind)
	{
		for (auto* renderer : renderers)
		{
			if (!renderer)
			{
				continue;
			}

			Actor* actor = renderer->GetOwner();
			const bool liveActor = IsActorHierarchyActive(actor);

			if (!liveActor || renderer->IsDestroyed())
			{
				continue;
			}

			if (!renderer->IsVisible() || !renderer->IsConfigured())
			{
				continue;
			}

			// Update the stored information for the current renderer component before submitting.
			if (!renderer->SynchronizeRenderResources())
			{
				return false;
			}

			const RenderSpace space = renderer->GetRenderSpace();

			// Do not submit SkinnedMeshRenderer for Canvas preview or non-world space rendering.
			if (kind == SourceKind::SkinnedMesh && (canvasPreview || space != RenderSpace::World))
			{
				continue;
			}

			if (policy.canvasViewRoot)
			{
				if (!policy.canvasViewRoot->ContainsRenderer(renderer))
				{
					continue;
				}
			}
			else
			{
				const bool excludeWorld = policy.renderSpaceFilter == RenderSpaceFilter::ScreenOnly && space == RenderSpace::World;
				const bool excludeScreen = policy.renderSpaceFilter == RenderSpaceFilter::WorldOnly && space == RenderSpace::Screen;

				if (excludeWorld || excludeScreen)
				{
					continue;
				}
			}

			SubmissionSource source;
			source.component = renderer;
			source.kind = kind;
			source.screen = canvasPreview || space == RenderSpace::Screen;

			if (!source.screen && !camera)
			{
				continue;
			}

			if (source.screen)
			{
				source.orderPath = renderer->BuildCanvasSortPath();
			}

			sources.push_back(std::move(source));
		}

		return true;
	};

	//------------------------------------------------------------------------------------------------
	// 1. Gather all the renderers and their submission sources for the current frame.
	//------------------------------------------------------------------------------------------------
	if (!gather(m_meshRenderers, SourceKind::Mesh) ||
		!gather(m_skinnedMeshRenderers, SourceKind::SkinnedMesh) ||
		!gather(m_spriteRenderers, SourceKind::Sprite) ||
		!gather(m_uiRenderers, SourceKind::UI))
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// 2. Collect the skinning palette of all SkinnedMeshRenderer submissions.
	//------------------------------------------------------------------------------------------------
	// Check the total size of the skinning palettes to ensure it does not exceed the maximum allowed size.
	const std::size_t paletteLimit = (std::min)(
		frame.skinningPalettes.max_size(),
		static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()));

	std::size_t paletteEnd = frame.skinningPalettes.size();

	if (paletteEnd > paletteLimit)
	{
		return false;
	}

	// Collect the skinning palettes from all SkinnedMeshRenderer submissions
	// and append them to the frame's skinningPalettes vector.
	for (const auto& source : sources)
	{
		if (source.kind != SourceKind::SkinnedMesh)
		{
			continue;
		}

		const auto* renderer = static_cast<const SkinnedMeshRenderer*>(source.component);

		for (const auto& submesh : renderer->GetSubmeshes())
		{
			if (submesh.palette.size() > paletteLimit - paletteEnd)
			{
				return false;
			}

			paletteEnd += submesh.palette.size();
		}
	}

	frame.skinningPalettes.reserve(paletteEnd);

	//------------------------------------------------------------------------------------------------
	// 3. Sort the submission sources for rendering order.
	//	  World-Space submissions always precede Screen-Space submissions.
	//	  Screen-Space submissions are sorted by the order path to preserve Canvas hierarchy.
	//------------------------------------------------------------------------------------------------
	std::stable_sort(
		sources.begin(), sources.end(),
		[](const SubmissionSource& left, const SubmissionSource& right)
		{
			// World-Space submissions always precede Screen-Space submissions.
			if (left.screen != right.screen)
			{
				return !left.screen;
			}

			// For screen-space submissions, sort by the order path to preserve Canvas hierarchy.
			return left.screen && left.orderPath < right.orderPath;
		});

	//------------------------------------------------------------------------------------------------
	// 4. Construct the RenderViews for the current frame.
	//------------------------------------------------------------------------------------------------
	if (camera && !canvasPreview && policy.renderSpaceFilter != RenderSpaceFilter::ScreenOnly)
	{
		// Construct the main scene view for world-space rendering.
		rendering::RenderView sceneView;
		sceneView.id = sceneViewId;
		sceneView.output = output;
		sceneView.view = camera->viewMatrix;
		sceneView.projection = camera->projMatrix;
		sceneView.cameraPosition = camera->position;
		sceneView.exposure = 1.0f; // Default exposure; make it editable in the future.
		frame.views.push_back(sceneView);

		// Set the directional light for the scene view if it is enabled.
		if (light.enabled)
		{
			rendering::DirectionalLight input;
			input.lightDirection[0] = light.direction.x;
			input.lightDirection[1] = light.direction.y;
			input.lightDirection[2] = light.direction.z;

			input.lightColor[0] = light.color.x * light.intensity;
			input.lightColor[1] = light.color.y * light.intensity;
			input.lightColor[2] = light.color.z * light.intensity;

			input.ambient[0] = light.ambient;
			input.ambient[1] = light.ambient;
			input.ambient[2] = light.ambient;

			frame.directionalLight = input;
		}
	}

	rendering::RenderView overlay;
	overlay.id = overlayViewId;
	overlay.kind = rendering::ViewKind::Overlay;
	overlay.output = output;

	if (canvasPreview && camera)
	{// For Canvas preview, use the camera's view and projection matrices for the overlay view.
	 // (Common framework systemn should not familiar with the editor only concept honestly.....)
		overlay.view = camera->viewMatrix;
		overlay.projection = camera->projMatrix;
		overlay.cameraPosition = camera->position;
	}
	else
	{// For normal overlay view, use an orthographic projection that matches the viewport size.
		overlay.projection = Matrix4x4::CreateOrthographic(
			static_cast<float>(width),
			static_cast<float>(height),
			-1,
			100);

		if (camera)
		{
			overlay.cameraPosition = camera->position;
		}
	}

	// Always provide a background/Screen View, including no Main Camera.
	frame.views.push_back(overlay);

	//------------------------------------------------------------------------------------------------
	// 5. Append every submitted object to the RenderObject which Renderer requires for rendering.
	//------------------------------------------------------------------------------------------------
	// Lambda function to append a RenderObject to the frame and record its submission source.
	const auto append = [&](
		rendering::RenderObject object,
		const SubmissionSource& source,
		const ComponentMaterialState& material,
		bool alphaMask)
	{
		// Set the view where the object will be rendered.
		object.targetViews.push_back(source.screen ? overlayViewId : sceneViewId);

		ExtractedObjectRecord record;
		record.objectIndex = frame.objects.size();
		record.actorGuid = source.component->GetOwner()->GetGuid();
		record.material = &material;
		record.alphaMask = alphaMask;
		record.viewId = source.screen ? overlayViewId : sceneViewId;

		frame.objects.push_back(std::move(object));
		m_records.push_back(record);
	};

	// Lambda function to append a MeshRenderer submission to the frame.
	const auto appendMesh = [&](
		const SubmissionSource& source,
		const SubmeshRenderTemplate& renderTemplate,
		const Matrix4x4& actorWorld,
		std::optional<rendering::SkinningPaletteRange> paletteRange)
		{
			rendering::RenderObject object;
			object.geometry = renderTemplate.meshDesc.meshHandle;
			object.material = renderTemplate.materialDesc.handle;
			object.world = renderTemplate.localToModel * actorWorld * worldToCanvas;
			object.tint = source.component->GetColor();
			object.castShadow = source.component->GetCastShadow() && !source.screen;
			object.drawRange.first = renderTemplate.meshDesc.startIndex;
			object.drawRange.baseVertex = renderTemplate.meshDesc.baseVertex;
			object.skinningPalette = paletteRange;

			// Bounds calculation for dynamic vertex data is not supported,
			// so only set the world bounds if there is no skinning palette.
			if (!paletteRange)
			{
				SetWorldBounds(
					object,
					renderTemplate.meshDesc.boundsCenter,
					renderTemplate.meshDesc.boundsRadius,
					renderTemplate.meshDesc.boundsRevision,
					object.world);
			}

			append(std::move(object), source, renderTemplate.materialDesc, false);
		};

	CameraInfo emptyCamera;
	const CameraInfo& proxyCamera = camera ? *camera : emptyCamera;

	for (const SubmissionSource& source : sources)
	{
		// MeshRenderer submissions.
		if (source.kind == SourceKind::Mesh)
		{
			auto* renderer = static_cast<MeshRenderer*>(source.component);
			const auto& proxy = renderer->GetRenderProxy();

			for (const auto& renderTemplate : renderer->GetRenderTemplates())
			{
				appendMesh(source, renderTemplate, proxy.common.worldMatrix, std::nullopt);
			}

			continue;
		}

		// SkinnedMeshRenderer submissions.
		if (source.kind == SourceKind::SkinnedMesh)
		{
			const auto* renderer = static_cast<const SkinnedMeshRenderer*>(source.component);
			const auto* transform = renderer->GetOwner()->GetComponentByClass<Transform>();
			const Matrix4x4 actorWorld = transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();

			for (const auto& submesh : renderer->GetSubmeshes())
			{
				std::optional<rendering::SkinningPaletteRange> paletteRange;

				if (!submesh.palette.empty())
				{
					// Set the skinning palette range for the submesh in the frame's skinningPalettes vector.
					rendering::SkinningPaletteRange range;

					range.begin = static_cast<std::uint32_t>(frame.skinningPalettes.size());

					frame.skinningPalettes.insert(
						frame.skinningPalettes.end(),
						submesh.palette.begin()
						, submesh.palette.end());

					range.end = static_cast<std::uint32_t>(frame.skinningPalettes.size());

					paletteRange = range;
				}

				appendMesh(source, submesh.renderTemplate, actorWorld, paletteRange);
			}

			continue;
		}

		if (!context->pRenderAssets)
		{
			continue;
		}

		// SpriteRenderer and UIRenderer submissions are handled similarly,
		// as they both render quads with materials.
		const auto appendQuad = [&](
			const CommonRendererProxy& common,
			const ComponentMaterialState& material,
			rendering::BillboardMode billboard,
			bool castShadow)
		{
			Matrix4x4 world = common.worldMatrix * worldToCanvas;

			rendering::RenderObject object;
			object.geometry = context->pRenderAssets->Quad();	// Use built-in quad geometry for SpriteRenderer and UIRenderer.
			object.material = material.handle;
			object.world = world;
			object.tint = common.color;
			object.billboard = billboard;
			object.castShadow = castShadow && !source.screen;
			
			// Set the world bounds for the quad based on the material's LocalQuadTransform parameter.
			if (context->pRenderer)
			{
				rendering::GeometryData snapshot;
				const auto queried = context->pRenderer->GetGeometryData(object.geometry, snapshot);

				if (queried.IsSuccess())
				{
					const Vector4 local = LocalQuadTransform(material);
					Vector3 center = snapshot.boundsCenter;
					center.x = center.x * local.x + local.z;
					center.y = center.y * local.y + local.w;
					const float localStretch = (std::max)({ std::abs(local.x), std::abs(local.y), 1.0f });
					SetWorldBounds(
						object,
						center,
						snapshot.boundsRadius * localStretch,
						snapshot.revision,
						world);
				}
			}

			// Append the configured RenderObject to the frame.
			append(
				std::move(object),
				source,
				material,
				true);
		};

		if (source.kind == SourceKind::Sprite)
		{// SpriteRenderer submission.
			auto* renderer = static_cast<SpriteRenderer*>(source.component);
			const auto& proxy = renderer->GetRenderProxy(proxyCamera);
			const auto mode = static_cast<rendering::BillboardMode>(renderer->GetBillboardType());
			appendQuad(
				proxy.common,
				renderer->GetRenderTemplate().materialDesc,
				mode,
				renderer->GetCastShadow());
		}
		else
		{// UIRenderer submission.
			auto* renderer = static_cast<UIRenderer*>(source.component);
			const auto& proxy = renderer->GetRenderProxy();

			for (const UIRenderElement& element : renderer->GetRenderTemplate())
			{
				appendQuad(
					proxy.common,
					element.materialDesc,
					rendering::BillboardMode::None,
					false);
			}
		}
	}

	return true;
}
