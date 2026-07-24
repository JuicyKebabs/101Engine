#pragma once

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
};