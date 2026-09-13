#pragma once
#include "Engine/Core/GUID/Guid.h"

// Content identity, compared only for equality. It is not a schema or Handle generation.
class DefinitionRevision
{
public:
	static DefinitionRevision Generate();
	static bool TryParse(const std::string& text, DefinitionRevision& outRevision);
	bool IsValid() const { return m_guid.IsValid(); }
	std::string ToString() const { return m_guid.ToString(); }
	friend bool operator==(const DefinitionRevision&, const DefinitionRevision&) = default;

private:
	Guid m_guid;
};

