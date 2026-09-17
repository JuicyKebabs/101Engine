#include "TagManagementWorkflow.h"

#include "Document/EditorDocumentManager.h"
#include "Engine/Actor/Actor.h"
#include "Engine/Project/ProjectSettings.h"
#include "Engine/Resource/AssetManager.h"
#include "Engine/Scene/SceneBase.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_set>

namespace
{
	bool ContainsSerializedTag(const nlohmann::json& value, std::string_view name)
	{
		if (value.is_object())
		{
			for (auto it = value.begin(); it != value.end(); ++it)
			{
				if (it.key() == "tag" && it.value().is_string() && it.value().get<std::string>() == name)
				{
					return true;
				}

				if (ContainsSerializedTag(it.value(), name))
				{
					return true;
				}
			}
		}
		else if (value.is_array())
		{
			for (const auto& child : value)
			{
				if (ContainsSerializedTag(child, name))
				{
					return true;
				}
			}
		}

		return false;
	}

	TagManagementResult ValidateAvailable(
		std::string_view input,
		const ProjectSettings& settings,
		std::string& normalized,
		std::string_view ignored = {})
	{
		TagManagementResult result;

		if (!TagRegistry::NormalizeUserTagName(input, normalized, &result.error))
		{
			return result;
		}

		for (const std::string& existing : settings.GetUserTags())
		{
			if (existing == ignored)
			{
				continue;
			}

			if (TagRegistry::NamesEqualCaseInsensitive(existing, normalized))
			{
				result.error = "A Tag with this name already exists.";
				return result;
			}

			if (CalcTagId(existing) == CalcTagId(normalized))
			{
				result.error = "The Tag name has an FNV-1a collision with '" + existing + "'.";
				return result;
			}
		}

		for (const auto& [id, existing] : TagRegistry::Get().GetRegisteredTags())
		{
			if (existing == ignored || id == TAG_NONE)
			{
				continue;
			}

			if (TagRegistry::NamesEqualCaseInsensitive(existing, normalized))
			{
				result.error = "A registered Tag with this name already exists.";
				return result;
			}

			if (id == CalcTagId(normalized))
			{
				result.error = "The Tag name has an FNV-1a collision with '" + existing + "'.";
				return result;
			}
		}

		return result;
	}
}

TagManagementResult TagManagementWorkflow::Create(
	std::string_view name,
	ProjectSettings& settings,
	const std::string& settingsPath)
{
	std::string normalized;
	TagManagementResult result = ValidateAvailable(name, settings, normalized);

	if (!result)
	{
		return result;
	}

	auto tags = settings.GetUserTags();
	tags.push_back(normalized);
	std::sort(tags.begin(), tags.end());
	ProjectSettings candidate = settings;
	candidate.SetUserTags(std::move(tags));

	if (!candidate.Save(settingsPath, &result.error))
	{
		return result;
	}

	if (!TagRegistry::Get().RegisterUserTag(normalized, nullptr, &result.error))
	{
		return result;
	}

	settings = std::move(candidate);
	return result;
}

TagManagementResult TagManagementWorkflow::Rename(
	std::string_view oldName,
	std::string_view newName,
	ProjectSettings& settings,
	const std::string& settingsPath,
	const AssetManager& assets,
	const EditorDocumentManager& documents)
{
	TagManagementResult result;
	const auto old = std::find(settings.GetUserTags().begin(), settings.GetUserTags().end(), oldName);

	if (old == settings.GetUserTags().end())
	{
		result.error = "Only project-defined Tags can be renamed.";
		return result;
	}

	std::string normalized;
	result = ValidateAvailable(newName, settings, normalized, oldName);

	if (!result)
	{
		return result;
	}

	if (normalized == oldName)
	{
		result.error = "The Tag name is unchanged.";
		return result;
	}

	result = FindUsages(oldName, assets, documents);

	if (!result)
	{
		return result;
	}

	auto tags = settings.GetUserTags();
	*std::find(tags.begin(), tags.end(), oldName) = normalized;
	std::sort(tags.begin(), tags.end());
	ProjectSettings candidate = settings;
	candidate.SetUserTags(std::move(tags));

	if (!candidate.Save(settingsPath, &result.error))
	{
		return result;
	}

	if (!TagRegistry::Get().RegisterUserTag(normalized, nullptr, &result.error))
	{
		return result;
	}

	TagRegistry::Get().UnregisterUserTag(oldName);
	settings = std::move(candidate);
	return result;
}

TagManagementResult TagManagementWorkflow::Delete(
	std::string_view name,
	ProjectSettings& settings,
	const std::string& settingsPath,
	const AssetManager& assets,
	const EditorDocumentManager& documents)
{
	TagManagementResult result;
	auto tags = settings.GetUserTags();
	const auto item = std::find(tags.begin(), tags.end(), name);

	if (item == tags.end())
	{
		result.error = "Only project-defined Tags can be deleted.";
		return result;
	}

	result = FindUsages(name, assets, documents);

	if (!result)
	{
		return result;
	}

	tags.erase(item);
	ProjectSettings candidate = settings;
	candidate.SetUserTags(std::move(tags));

	if (!candidate.Save(settingsPath, &result.error))
	{
		return result;
	}

	TagRegistry::Get().UnregisterUserTag(name);
	settings = std::move(candidate);
	return result;
}

TagManagementResult TagManagementWorkflow::FindUsages(
	std::string_view name,
	const AssetManager& assets,
	const EditorDocumentManager& documents)
{
	TagManagementResult result;
	std::unordered_set<Guid> openAssets;

	for (const EditorDocumentInfo& info : documents.GetDocuments())
	{
		const IEditorDocument* document = documents.FindDocument(info.id);

		if (!document || !document->GetWorkingScene())
		{
			continue;
		}

		if (info.sourceAssetGuid.IsValid())
		{
			openAssets.insert(info.sourceAssetGuid);
		}

		for (Actor* actor : document->GetWorkingScene()->GetAllActors())
		{
			if (actor && !actor->IsDestroyed() && TagRegistry::Get().GetName(actor->GetTag()) == name)
			{
				result.usages.push_back({"Open document '" + info.displayName + "', Actor '" + actor->GetName() + "'"});
			}
		}
	}

	for (AssetType type : {AssetType::Scene, AssetType::ActorImprint})
	{
		for (const AssetEntry& entry : assets.GetAssetEntries(type))
		{
			if (openAssets.contains(entry.guid))
			{
				continue;
			}

			try
			{
				std::ifstream input(assets.GetAssetPath(entry.guid));

				if (!input)
				{
					result.error = "Could not inspect Tag usage in '" + entry.relativePath + "'.";
					return result;
				}

				const nlohmann::json root = nlohmann::json::parse(input);

				if (ContainsSerializedTag(root, name))
				{
					result.usages.push_back({entry.relativePath});
				}
			}
			catch (const std::exception& exception)
			{
				result.error = "Could not inspect Tag usage in '" + entry.relativePath + "': " + exception.what();
				return result;
			}
		}
	}

	if (!result.usages.empty())
	{
		result.error = "The Tag is in use and cannot be changed.";
	}

	return result;
}
