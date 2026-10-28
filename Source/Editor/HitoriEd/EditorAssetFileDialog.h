#pragma once

#include <filesystem>
#include <shobjidl.h>
#include <wrl/client.h>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")

namespace EditorAssetFileDialog
{
    inline constexpr GUID SceneId = { 0x3ba58ff1, 0xbde7, 0x4d84, { 0x93, 0xba, 0x5c, 0x89, 0x3b, 0x27, 0x61, 0x10 } };
    inline constexpr GUID SequenceId = { 0x5b68a822, 0x66cd, 0x4d38, { 0x8a, 0xee, 0x62, 0x6e, 0x53, 0x15, 0x02, 0xfa } };

    inline HRESULT Show(bool Save, const GUID& ClientId, const std::filesystem::path& Folder,
        const wchar_t* FilterName, const wchar_t* Pattern, const wchar_t* Extension,
        const wchar_t* FileName, std::filesystem::path& Result)
    {
        const HRESULT Init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(Init) && Init != RPC_E_CHANGED_MODE) return Init;
        struct FComScope
        {
            bool Initialized;
            ~FComScope() { if (Initialized) CoUninitialize(); }
        } Scope{ SUCCEEDED(Init) };

        Microsoft::WRL::ComPtr<IFileDialog> Dialog;
        HRESULT Status = CoCreateInstance(Save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog,
            nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(Dialog.GetAddressOf()));
        if (FAILED(Status)) return Status;
        Status = Dialog->SetClientGuid(ClientId);
        if (FAILED(Status)) return Status;
        FILEOPENDIALOGOPTIONS Options{};
        Status = Dialog->GetOptions(&Options);
        if (FAILED(Status)) return Status;
        Status = Dialog->SetOptions(Options | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR |
            FOS_PATHMUSTEXIST | (Save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
        if (FAILED(Status)) return Status;
        const COMDLG_FILTERSPEC Filter{ FilterName, Pattern };
        Status = Dialog->SetFileTypes(1, &Filter);
        if (FAILED(Status)) return Status;
        Status = Dialog->SetDefaultExtension(Extension);
        if (FAILED(Status)) return Status;
        Microsoft::WRL::ComPtr<IShellItem> InitialFolder;
        Status = SHCreateItemFromParsingName(Folder.c_str(), nullptr, IID_PPV_ARGS(InitialFolder.GetAddressOf()));
        if (FAILED(Status)) return Status;
        Status = Dialog->SetFolder(InitialFolder.Get());
        if (FAILED(Status)) return Status;
        if (Save)
        {
            Status = Dialog->SetFileName(FileName);
            if (FAILED(Status)) return Status;
        }
        Status = Dialog->Show(GetActiveWindow());
        if (FAILED(Status)) return Status;
        Microsoft::WRL::ComPtr<IShellItem> Item;
        Status = Dialog->GetResult(Item.GetAddressOf());
        if (FAILED(Status)) return Status;
        PWSTR Path = nullptr;
        Status = Item->GetDisplayName(SIGDN_FILESYSPATH, &Path);
        if (SUCCEEDED(Status)) Result = Path;
        CoTaskMemFree(Path);
        return Status;
    }
}
