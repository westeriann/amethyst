#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace plat {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

std::string appDataDir() {
    PWSTR path = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
        dir = std::wstring(path) + L"\\Amethyst";
        CoTaskMemFree(path);
    } else {
        dir = L".\\Amethyst";
    }
    CreateDirectoryW(dir.c_str(), nullptr);
    return narrow(dir);
}

std::string exeDir() {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    std::wstring p(buf, n);
    auto slash = p.find_last_of(L"\\/");
    return narrow(slash == std::wstring::npos ? L"." : p.substr(0, slash));
}

static std::string itemPath(IShellItem* item) {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
        out = narrow(p);
        CoTaskMemFree(p);
    }
    return out;
}

std::vector<std::string> pickFiles(void* hwnd, const wchar_t* title, const FilterList& filters, bool multi) {
    std::vector<std::string> out;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return out;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | (multi ? FOS_ALLOWMULTISELECT : 0));
    std::vector<COMDLG_FILTERSPEC> spec;
    for (auto& f : filters) spec.push_back({f.first.c_str(), f.second.c_str()});
    if (!spec.empty()) dlg->SetFileTypes((UINT)spec.size(), spec.data());
    dlg->SetTitle(title);
    if (SUCCEEDED(dlg->Show((HWND)hwnd))) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(dlg->GetResults(&items))) {
            DWORD count = 0;
            items->GetCount(&count);
            for (DWORD i = 0; i < count; i++) {
                IShellItem* it = nullptr;
                if (SUCCEEDED(items->GetItemAt(i, &it))) {
                    auto p = itemPath(it);
                    if (!p.empty()) out.push_back(p);
                    it->Release();
                }
            }
            items->Release();
        }
    }
    dlg->Release();
    return out;
}

std::string pickFolder(void* hwnd, const wchar_t* title) {
    std::string out;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return out;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(title);
    if (SUCCEEDED(dlg->Show((HWND)hwnd))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(dlg->GetResult(&it))) {
            out = itemPath(it);
            it->Release();
        }
    }
    dlg->Release();
    return out;
}

std::string pickSaveFile(void* hwnd, const wchar_t* title, const FilterList& filters, const std::string& defaultName,
                         const wchar_t* defaultExt) {
    std::string out;
    IFileSaveDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) return out;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
    std::vector<COMDLG_FILTERSPEC> spec;
    for (auto& f : filters) spec.push_back({f.first.c_str(), f.second.c_str()});
    if (!spec.empty()) dlg->SetFileTypes((UINT)spec.size(), spec.data());
    dlg->SetTitle(title);
    if (defaultExt) dlg->SetDefaultExtension(defaultExt);
    if (!defaultName.empty()) dlg->SetFileName(widen(defaultName).c_str());
    if (SUCCEEDED(dlg->Show((HWND)hwnd))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(dlg->GetResult(&it))) {
            out = itemPath(it);
            it->Release();
        }
    }
    dlg->Release();
    return out;
}

static std::string knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) {
        out = narrow(p);
        CoTaskMemFree(p);
    }
    return out;
}
std::string documentsDir() { return knownFolder(FOLDERID_Documents); }
std::string videosDir() { return knownFolder(FOLDERID_Videos); }

void openPath(const std::string& path) { ShellExecuteW(nullptr, L"open", widen(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

void revealInExplorer(const std::string& path) {
    std::wstring args = L"/select,\"" + widen(path) + L"\"";
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

}  // namespace plat
