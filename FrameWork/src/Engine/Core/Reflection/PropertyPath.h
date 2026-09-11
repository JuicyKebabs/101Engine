#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// A normalized JSON Pointer identifying one reflected leaf property.
class PropertyPath
{
public:
	static std::optional<PropertyPath> FromString(std::string_view path);
	static std::optional<PropertyPath> FromMembers(
		const std::vector<std::string>& members);

	const std::string& ToString() const { return m_path; }
	const std::vector<std::string>& GetMembers() const { return m_members; }

	friend bool operator==(const PropertyPath&, const PropertyPath&) = default;

private:
	PropertyPath(std::string path, std::vector<std::string> members)
		: m_path(std::move(path)), m_members(std::move(members))
	{}

	std::string m_path;
	std::vector<std::string> m_members;
};
