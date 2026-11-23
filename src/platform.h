// Small Windows helpers: UTF-8 <-> UTF-16, app folders, native file dialogs.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace plat {

std::wstring widen(const std::string& s);
std::string narrow(const std::wstring& s);

std::string appDataDir();  // %APPDATA%\Amethyst (created on demand)
std::string exeDir();

using FilterList = std::vector<std::pair<std::wstring, std::wstring>>;  // { label, "*.milk;*.prjm" }
std::vector<std::string> pickFiles(void* hwnd, const wchar_t* title, const FilterList& filters, bool multi);
std::string pickFolder(void* hwnd, const wchar_t* title);
std::string pickSaveFile(void* hwnd, const wchar_t* title, const FilterList& filters, const std::string& defaultName,
                         const wchar_t* defaultExt);
void revealInExplorer(const std::string& path);
void openPath(const std::string& path);
std::string documentsDir();  // ...\Documents
std::string videosDir();     // ...\Videos

}  // namespace plat
