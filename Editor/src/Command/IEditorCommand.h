#pragma once
#include "Engine/Scene/StructuralMutationResult.h"

//----------------------------------------------------------------
// IEditorCommand class
// Interface for editor commands that can be executed and undone.
//----------------------------------------------------------------

class IEditorCommand
{
public:
	virtual ~IEditorCommand() = default;

	virtual bool Execute() = 0;
	virtual bool Undo() = 0;
	const StructuralMutationResult& GetStructuralResult() const { return m_structuralResult; }
protected:
	StructuralMutationResult m_structuralResult;
};
