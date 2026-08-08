#pragma once
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <mutex>

namespace UI {
    class TextureManager {
    public:
        static void Initialize(ID3D11Device* device);
        static void Shutdown();
        static ID3D11ShaderResourceView* GetFileIcon(const std::string& path);

    private:
        static ID3D11Device* s_Device;
        static std::unordered_map<std::string, ID3D11ShaderResourceView*> s_Cache;
        static std::mutex s_Mutex;
        static ID3D11ShaderResourceView* s_DefaultIcon;

        static ID3D11ShaderResourceView* LoadIconFromHICON(HICON hIcon);
        static ID3D11ShaderResourceView* LoadDefaultIcon();
    };
}
