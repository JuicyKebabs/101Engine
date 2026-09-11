#include "PropertyPath.h"

namespace
{
	std::string EscapeMember(std::string_view member)
	{
		std::string escaped;
		for (char character : member)
		{
			if (character == '~') escaped += "~0";
			else if (character == '/') escaped += "~1";
			else escaped += character;
		}
		return escaped;
	}
}

std::optional<PropertyPath> PropertyPath::FromString(std::string_view path)
{
	if (path.empty() || path.front() != '/') return std::nullopt;

	std::vector<std::string> members;
	std::size_t begin = 1;
	while (begin <= path.size())
	{
		const std::size_t end = path.find('/', begin);
		std::size_t encodedLength = path.size() - begin;
		if (end != std::string_view::npos)
		{
			encodedLength = end - begin;
		}
		const std::string_view encoded = path.substr(begin, encodedLength);
		if (encoded.empty()) return std::nullopt;

		std::string member;
		for (std::size_t i = 0; i < encoded.size(); ++i)
		{
			if (encoded[i] != '~')
			{
				member += encoded[i];
				continue;
			}

			if (++i >= encoded.size()) return std::nullopt;
			if (encoded[i] == '0') member += '~';
			else if (encoded[i] == '1') member += '/';
			else return std::nullopt;
		}

		members.push_back(std::move(member));
		if (end == std::string_view::npos) break;
		begin = end + 1;
	}

	return FromMembers(members);
}

std::optional<PropertyPath> PropertyPath::FromMembers(
	const std::vector<std::string>& members)
{
	if (members.empty()) return std::nullopt;

	std::string path;
	for (const std::string& member : members)
	{
		if (member.empty()) return std::nullopt;
		path += '/';
		path += EscapeMember(member);
	}

	return PropertyPath(std::move(path), members);
}
