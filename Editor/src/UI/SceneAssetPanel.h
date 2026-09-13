#pragma once

#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetManager.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class SceneAssetPanel
{
public:
	struct Callbacks
	{
		std::function<bool(std::string_view)> onCreate;
		std::function<bool(const Guid&)> onOpen;
		std::function<bool(const Guid&, std::string_view)> onRename;
		std::function<bool(const Guid&)> onSetStartup;
		bool canModify = true;
	};

	void Render(const AssetManager& assets, const Guid& startupSceneGuid, const Callbacks& callbacks);
	static std::vector<AssetEntry> GetVisibleEntries(const AssetManager& assets);
	void RequestCreateDialog() { m_openCreatePopup = true; }
	void Select(const Guid& guid) { m_selectedGuid = guid; }
	void SetDiagnostic(std::string diagnostic) { m_diagnostic = std::move(diagnostic); }

private:
	Guid m_selectedGuid;
	Guid m_renameGuid;
	std::string m_diagnostic;
	bool m_openCreatePopup = false;
	bool m_openRenamePopup = false;
	char m_name[256]{};
};
