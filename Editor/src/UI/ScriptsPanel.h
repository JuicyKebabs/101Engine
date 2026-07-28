#pragma once
#include <functional>
#include <string>
#include <vector>
//---------------------------------------------------------------
// ScriptsPanel class
// This class represents a panel that displays a list of scripts.
// User ca \n edit, add, and remove scripts from the panel.
//---------------------------------------------------------------

class SceneBase; // Forward declaration of SceneBase class

class ScriptsPanel
{
public:

	// Struct to hold the callbacks for the panel
	struct Callbacks
	{
		std::function<void(const std::string& name)> onDelete;	// Callback for when a script is deleted	
		std::function<void(const std::string& name)> onOpen;	// Callback for when a script is opened
	};

	void Render(const Callbacks& callbacks, SceneBase* scene);

private:

	// Struct to hold the information about a script entry
	struct ScriptEntry
	{
		std::string name;	// Name of the script
		bool isBehavior;	// Flag to indicate if the script is registered in ComponentRegistry
	};

	std::vector<ScriptEntry> ScanScripts();	// Function to scan and return a list of scripts

	std::string m_pendingDeleteName;	// Name of the script that is pending deletion
	bool m_showDeleteConfirm;			// Flag to indicate if the delete confirmation dialog should be shown
	bool m_inUseByScene;				// Flag to indicate if the script is in use by the scene
};