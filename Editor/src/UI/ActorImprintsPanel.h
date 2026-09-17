#pragma once

#include "Engine/Core/GUID/Guid.h"
#include "Engine/Resource/AssetManager.h"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class ActorImprintsPanel
{
public:
	struct Callbacks
	{
		std::function<bool(std::string_view name)> onCreate;
		std::function<bool(const Guid& assetGuid)> onEdit;
		std::function<bool(const Guid& assetGuid)> onDelete;
		bool canModify = true;
	};

	void Render(const AssetManager& assets, const Callbacks& callbacks);
	static std::vector<AssetEntry> GetVisibleEntries(const AssetManager& assets);
	void RequestCreateDialog() { m_openCreatePopup = true; }
	bool IsCreateDialogRequested() const { return m_openCreatePopup; }
	void RequestDeleteConfirmation(const Guid& assetGuid)
	{
		m_pendingDeleteGuid = assetGuid;
		m_openDeletePopup = assetGuid.IsValid();
	}
	const Guid& GetPendingDeleteGuid() const { return m_pendingDeleteGuid; }
	static bool DispatchCreate(const Callbacks& callbacks, std::string_view name)
	{
		return callbacks.canModify && callbacks.onCreate && callbacks.onCreate(name);
	}
	static bool DispatchEdit(const Callbacks& callbacks, const Guid& assetGuid)
	{
		return callbacks.canModify && callbacks.onEdit && callbacks.onEdit(assetGuid);
	}
	static bool DispatchDelete(const Callbacks& callbacks, const Guid& assetGuid)
	{
		return callbacks.canModify && callbacks.onDelete && callbacks.onDelete(assetGuid);
	}
	void Select(const Guid& assetGuid) { m_selectedAssetGuid = assetGuid; }
	const Guid& GetSelectedAssetGuid() const { return m_selectedAssetGuid; }

private:
	Guid m_selectedAssetGuid;
	Guid m_pendingDeleteGuid;
	bool m_openCreatePopup = false;
	bool m_openDeletePopup = false;
	char m_name[256]{};
};
