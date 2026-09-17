#include "ActorImprintReferenceCodec.h"
#include "Engine/Core/Debug/Debug.h"
#include "nlohmann/json.hpp"
#include <utility>

ActorImprintReferenceCodec::ActorImprintReferenceCodec(ActorGuids actorGuids, ActorImprintReferenceMode mode)
	: m_actorGuids(std::move(actorGuids)), m_mode(mode)
{
	for (const auto& [id, guid] : m_actorGuids)
	{
		if (id == 0 || !guid.IsValid() || !m_localIds.emplace(guid, id).second)
		{
			m_valid = false;
		}
	}
}

bool ActorImprintReferenceCodec::Serialize(
	const ActorReference& reference,
	const ActorReferenceSaveContext& context,
	nlohmann::json& outJson) const
{
	if (!m_valid)
	{
		DBG("Actor reference: InvalidGuid.");
		return false;
	}

	if (!reference.HasValue())
	{
		outJson = nullptr;
		return true;
	}

	const auto local = m_localIds.find(reference.GetGuid());
	const auto result = context.Validate(reference.GetGuid());

	if (!result)
	{
		return result;
	}

	if (local != m_localIds.end())
	{
		outJson = { { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", local->second } };
		return true;
	}

	if (m_mode != ActorImprintReferenceMode::Instance)
	{
		DBG("Actor reference: ActorNotFound.");
		return false;
	}

	outJson = { { "type", "ActorReference" }, { "scope", "scene" },
		{ "actorGuid", reference.GetGuid().ToString() } };
	return true;
}

bool ActorImprintReferenceCodec::Deserialize(
	const nlohmann::json& json,
	ActorReference& outReference) const
{
	if (!m_valid)
	{
		DBG("Actor reference: InvalidGuid.");
		return false;
	}

	if (json.is_null())
	{
		outReference.Clear();
		return true;
	}

	if (!json.is_object() || json.size() != 3 || !json.contains("type") ||
		!json["type"].is_string() || json["type"] != "ActorReference" ||
		!json.contains("scope") || !json["scope"].is_string())
	{
		DBG("Actor reference: InvalidJsonType.");
		return false;
	}

	ActorReference reference;

	if (json["scope"] == "local")
	{
		if (!json.contains("localObjectId"))
		{
			DBG("Actor reference: InvalidJsonType.");
			return false;
		}

		const auto& value = json["localObjectId"];

		if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() <= 0))
		{
			DBG("Actor reference: InvalidJsonType.");
			return false;
		}

		const LocalObjectId id = value.get<LocalObjectId>();
		const auto actor = m_actorGuids.find(id);

		if (id == 0 || actor == m_actorGuids.end())
		{
			DBG("Actor reference: ActorNotFound.");
			return false;
		}

		if (!reference.SetGuid(actor->second))
		{
			DBG("Actor reference: InvalidGuid.");
			return false;
		}
	}
	else if (json["scope"] == "scene")
	{
		if (m_mode != ActorImprintReferenceMode::Instance || !json.contains("actorGuid") ||
			!json["actorGuid"].is_string())
		{
			DBG("Actor reference: InvalidJsonType.");
			return false;
		}

		Guid guid;
		const std::string text = json["actorGuid"].get<std::string>();

		if (text.find('\0') != std::string::npos || !Guid::TryParse(text, guid))
		{
			DBG("Actor reference: InvalidGuid.");
			return false;
		}

		if (m_localIds.contains(guid))
		{
			DBG("Actor reference: InvalidJsonType.");
			return false;
		}

		if (!reference.SetGuid(guid))
		{
			DBG("Actor reference: InvalidGuid.");
			return false;
		}
	}
	else
	{
		DBG("Actor reference: InvalidJsonType.");
		return false;
	}

	outReference = reference;
	return true;
}

bool ActorImprintReferenceCodec::Resolve(
	ActorReference& reference,
	const ActorReferenceRestoreContext& context) const
{
	if (!m_valid)
	{
		DBG("Actor reference: InvalidGuid.");
		return false;
	}

	return GuidActorReferenceCodec().Resolve(reference, context);
}
