#include "ActorTag.h"

#include <Windows.h>
#include <cctype>

namespace
{
	std::wstring ToWide(std::string_view value)
	{
		const int size = MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);

		if (size <= 0)
		{
			return {};
		}

		std::wstring result(static_cast<std::size_t>(size), L'\0');
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), result.data(), size);
		return result;
	}
}

TagRegistry& TagRegistry::Get()
{
	static TagRegistry instance;
	return instance;
}

TagId TagRegistry::Register(std::string_view tagName)
{
	const TagId id = CalcTagId(tagName);
	const auto it = m_tags.find(id);

	if (it == m_tags.end() || it->second == tagName)
	{
		m_tags[id] = std::string(tagName);
	}

	return id;
}

bool TagRegistry::NormalizeUserTagName(std::string_view input, std::string& output, std::string* outError)
{
	auto fail = [&](const char* message)
	{
		if (outError)
		{
			*outError = message;
		}

		return false;
	};

	if (outError)
	{
		outError->clear();
	}

	output.assign(input);
	while (!output.empty() && std::isspace(static_cast<unsigned char>(output.front())))
		output.erase(output.begin());
	while (!output.empty() && std::isspace(static_cast<unsigned char>(output.back())))
		output.pop_back();

	if (output.empty())
	{
		return fail("Tag name must not be empty.");
	}

	if (output.size() > 64)
	{
		return fail("Tag name must not exceed 64 UTF-8 bytes.");
	}

	for (unsigned char c : output)
	{
		if (c < 0x20 || c == 0x7f)
		{
			return fail("Tag name must not contain control characters.");
		}
	}

	if (MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, output.data(), static_cast<int>(output.size()), nullptr, 0) == 0)
	{
		return fail("Tag name must be valid UTF-8.");
	}

	std::string folded = output;
	std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c)
	{
		return static_cast<char>(std::tolower(c));
	});

	if (folded == "none" || folded == "maincamera" || folded == "initialsky")
	{
		return fail("Tag name is reserved by the Engine.");
	}

	return true;
}

bool TagRegistry::NamesEqualCaseInsensitive(std::string_view left, std::string_view right)
{
	const std::wstring wideLeft = ToWide(left);
	const std::wstring wideRight = ToWide(right);

	if ((left.empty() || !wideLeft.empty()) && (right.empty() || !wideRight.empty()))
	{
		return CompareStringOrdinal(wideLeft.data(), static_cast<int>(wideLeft.size()), wideRight.data(),
				   static_cast<int>(wideRight.size()), TRUE) == CSTR_EQUAL;
	}

	return left == right;
}

bool TagRegistry::RegisterUserTag(std::string_view tagName, TagId* outId, std::string* outError)
{
	std::string normalized;

	if (!NormalizeUserTagName(tagName, normalized, outError))
	{
		return false;
	}

	for (const auto& [id, name] : m_tags)
	{
		if (NamesEqualCaseInsensitive(name, normalized))
		{
			if (name != normalized)
			{
				if (outError)
				{
					*outError = "Tag name differs only by letter case.";
				}

				return false;
			}

			if (outId)
			{
				*outId = id;
			}

			return true;
		}
	}

	const TagId id = CalcTagId(normalized);
	const auto collision = m_tags.find(id);

	if (collision != m_tags.end() && collision->second != normalized)
	{
		if (outError)
		{
			*outError = "Tag name has an FNV-1a collision with '" + collision->second + "'.";
		}

		return false;
	}

	m_tags[id] = normalized;

	if (outId)
	{
		*outId = id;
	}

	return true;
}

bool TagRegistry::ValidateUserTagSet(
	const std::vector<std::string>& input,
	std::vector<std::string>& normalized,
	std::string* outError)
{
	TagRegistry validation;
	std::vector<std::string> candidate;

	for (const std::string& value : input)
	{
		std::string name;

		if (!NormalizeUserTagName(value, name, outError))
		{
			return false;
		}

		if (validation.ContainsName(name))
		{
			if (outError)
			{
				*outError = "Tag list contains a duplicate name.";
			}

			return false;
		}

		if (!validation.RegisterUserTag(name, nullptr, outError))
		{
			return false;
		}

		candidate.push_back(std::move(name));
	}

	std::sort(candidate.begin(), candidate.end());
	normalized = std::move(candidate);
	return true;
}

bool TagRegistry::UnregisterUserTag(std::string_view tagName)
{
	const TagId id = CalcTagId(tagName);

	if (IsReserved(id))
	{
		return false;
	}

	const auto it = m_tags.find(id);

	if (it == m_tags.end() || it->second != tagName)
	{
		return false;
	}

	m_tags.erase(it);
	return true;
}

bool TagRegistry::ContainsName(std::string_view tagName) const
{
	const auto it = m_tags.find(CalcTagId(tagName));
	return it != m_tags.end() && it->second == tagName;
}

bool TagRegistry::IsReserved(TagId id) const
{
	return id == TAG_NONE || id == CalcTagId("MainCamera") ||
		id == CalcTagId("InitialSky");
}

TagId TagRegistry::GetId(std::string_view tagName)
{
	const TagId id = CalcTagId(tagName);
	const auto it = m_tags.find(id);

	if (it == m_tags.end())
	{
		m_tags[id] = std::string(tagName);
	}

	return id;
}

std::string TagRegistry::GetName(TagId id) const
{
	if (id == TAG_NONE)
	{
		return "None";
	}

	const auto it = m_tags.find(id);
	return it != m_tags.end() ? it->second : "UnknownTag";
}

std::vector<std::pair<TagId, std::string>> TagRegistry::GetRegisteredTags() const
{
	std::vector<std::pair<TagId, std::string>> tags;
	tags.reserve(m_tags.size() + 1);
	tags.emplace_back(TAG_NONE, "None");

	for (const auto& [id, name] : m_tags)
	{
		if (id != TAG_NONE)
		{
			tags.emplace_back(id, name);
		}
	}

	std::sort(tags.begin() + 1, tags.end(),
		[](const auto& left, const auto& right) { return left.second < right.second; });
	return tags;
}
