#include "ActorImprintReferenceCodec.h"
#include "nlohmann/json.hpp"
#include <utility>

ActorImprintReferenceCodec::ActorImprintReferenceCodec(ActorGuids actorGuids, ActorImprintReferenceMode mode)
	: m_actorGuids(std::move(actorGuids)), m_mode(mode)
{
	for (const auto& [id, guid] : m_actorGuids)
	{
		if (id == 0 || !guid.IsValid() || !m_localIds.emplace(guid, id).second) m_valid = false;
	}
}

ActorReferenceCodecResult ActorImprintReferenceCodec::Serialize(const ActorReference& reference,
	const ActorReferenceSaveContext& context, nlohmann::json& outJson) const
{
	if (!m_valid) return ActorReferenceCodecResult::InvalidGuid;
	if (!reference.HasValue())
	{
		outJson = nullptr;
		return ActorReferenceCodecResult::Success;
	}
	const auto local = m_localIds.find(reference.GetGuid());
	const auto result = context.Validate(reference.GetGuid());
	if (result != ActorReferenceCodecResult::Success) return result;
	if (local != m_localIds.end())
	{
		outJson = { { "type", "ActorReference" }, { "scope", "local" }, { "localObjectId", local->second } };
		return ActorReferenceCodecResult::Success;
	}
	if (m_mode != ActorImprintReferenceMode::Instance) return ActorReferenceCodecResult::ActorNotFound;
	outJson = { { "type", "ActorReference" }, { "scope", "scene" },
		{ "actorGuid", reference.GetGuid().ToString() } };
	return ActorReferenceCodecResult::Success;
}

ActorReferenceCodecResult ActorImprintReferenceCodec::Deserialize(const nlohmann::json& json,
	ActorReference& outReference) const
{
	if (!m_valid) return ActorReferenceCodecResult::InvalidGuid;
	if (json.is_null())
	{
		outReference.Clear();
		return ActorReferenceCodecResult::Success;
	}
	if (!json.is_object() || json.size() != 3 || !json.contains("type") ||
		!json["type"].is_string() || json["type"] != "ActorReference" ||
		!json.contains("scope") || !json["scope"].is_string())
		return ActorReferenceCodecResult::InvalidJsonType;
	ActorReference reference;
	if (json["scope"] == "local")
	{
		if (!json.contains("localObjectId")) return ActorReferenceCodecResult::InvalidJsonType;
		const auto& value = json["localObjectId"];
		if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<std::int64_t>() <= 0))
			return ActorReferenceCodecResult::InvalidJsonType;
		const LocalObjectId id = value.get<LocalObjectId>();
		const auto actor = m_actorGuids.find(id);
		if (id == 0 || actor == m_actorGuids.end()) return ActorReferenceCodecResult::ActorNotFound;
		if (!reference.SetGuid(actor->second)) return ActorReferenceCodecResult::InvalidGuid;
	}
	else if (json["scope"] == "scene")
	{
		if (m_mode != ActorImprintReferenceMode::Instance || !json.contains("actorGuid") ||
			!json["actorGuid"].is_string()) return ActorReferenceCodecResult::InvalidJsonType;
		Guid guid;
		const std::string text = json["actorGuid"].get<std::string>();
		if (text.find('\0') != std::string::npos || !Guid::TryParse(text, guid))
			return ActorReferenceCodecResult::InvalidGuid;
		if (m_localIds.contains(guid)) return ActorReferenceCodecResult::InvalidJsonType;
		if (!reference.SetGuid(guid)) return ActorReferenceCodecResult::InvalidGuid;
	}
	else return ActorReferenceCodecResult::InvalidJsonType;
	outReference = reference;
	return ActorReferenceCodecResult::Success;
}

ActorReferenceCodecResult ActorImprintReferenceCodec::Resolve(ActorReference& reference,
	const ActorReferenceRestoreContext& context) const
{
	if (!m_valid) return ActorReferenceCodecResult::InvalidGuid;
	return GuidActorReferenceCodec().Resolve(reference, context);
}
