#include "DefinitionRevision.h"
#include "Engine/Core/GUID/GuidGenerator.h"

DefinitionRevision DefinitionRevision::Generate()
{
	DefinitionRevision revision;
	revision.m_guid = GuidGenerator::Generate();
	return revision;
}

bool DefinitionRevision::TryParse(const std::string& text, DefinitionRevision& outRevision)
{
	Guid guid;
	if (text.find('\0') != std::string::npos || !Guid::TryParse(text, guid)) return false;
	outRevision.m_guid = guid;
	return true;
}
