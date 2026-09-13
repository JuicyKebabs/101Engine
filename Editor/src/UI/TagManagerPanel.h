#pragma once

#include <functional>
#include <string>
#include <vector>

class TagManagerPanel
{
public:
	struct Callbacks
	{
		std::function<bool(const std::string&)> onCreate;
		std::function<bool(const std::string&, const std::string&)> onRename;
		std::function<bool(const std::string&)> onDelete;
	};

	void Render(const std::vector<std::string>& userTags, const Callbacks& callbacks);

private:
	void OpenNamePopup(const char* title, std::string current = {});
	char m_nameBuffer[128]{};
	std::string m_target;
	std::string m_popupTitle;
	bool m_openNamePopup = false;
	bool m_openDeletePopup = false;
};
