#include "TextureManager.h"
#include <shellapi.h>
#include <vector>
#include <iostream>

namespace UI {
    ID3D11Device* TextureManager::s_Device = nullptr;
    std::unordered_map<std::string, ID3D11ShaderResourceView*> TextureManager::s_Cache;
    std::mutex TextureManager::s_Mutex;
    ID3D11ShaderResourceView* TextureManager::s_DefaultIcon = nullptr;

    void TextureManager::Initialize(ID3D11Device* device) {
        s_Device = device;
        s_DefaultIcon = LoadDefaultIcon();
    }

    void TextureManager::Shutdown() {
        std::lock_guard<std::mutex> lock(s_Mutex);
        for (auto& pair : s_Cache) {
            if (pair.second) {
                pair.second->Release();
            }
        }
        s_Cache.clear();
        
        if (s_DefaultIcon) {
            s_DefaultIcon->Release();
            s_DefaultIcon = nullptr;
        }
    }

    ID3D11ShaderResourceView* TextureManager::GetFileIcon(const std::string& path) {
        if (!s_Device) return nullptr;

        std::lock_guard<std::mutex> lock(s_Mutex);

        auto it = s_Cache.find(path);
        if (it != s_Cache.end()) {
            return it->second ? it->second : s_DefaultIcon;
        }

        
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
        std::wstring wpath(wlen, 0);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

        SHFILEINFOW sfi = {0};
        
        
        if (SHGetFileInfoW(wpath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON)) {
            if (sfi.hIcon) {
                ID3D11ShaderResourceView* srv = LoadIconFromHICON(sfi.hIcon);
                DestroyIcon(sfi.hIcon);
                if (srv) {
                    s_Cache[path] = srv;
                    return srv;
                }
            }
        }

        
        s_Cache[path] = nullptr;
        return s_DefaultIcon;
    }

    ID3D11ShaderResourceView* TextureManager::LoadDefaultIcon() {
        SHFILEINFOW sfi = {0};
        
        if (SHGetFileInfoW(L".exe", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
            if (sfi.hIcon) {
                ID3D11ShaderResourceView* srv = LoadIconFromHICON(sfi.hIcon);
                DestroyIcon(sfi.hIcon);
                return srv;
            }
        }
        return nullptr;
    }

    ID3D11ShaderResourceView* TextureManager::LoadIconFromHICON(HICON hIcon) {
        if (!s_Device) return nullptr;

        ICONINFO ii;
        if (!GetIconInfo(hIcon, &ii)) return nullptr;

        BITMAP bmp;
        GetObject(ii.hbmColor, sizeof(BITMAP), &bmp);
        
        int width = bmp.bmWidth;
        int height = bmp.bmHeight;

        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);

        
        BITMAPINFO bmi = {0};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; 
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        
        void* pBits = nullptr;
        HBITMAP hbmDib = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
        
        if (!hbmDib) {
            DeleteDC(hdcMem);
            ReleaseDC(NULL, hdcScreen);
            return nullptr;
        }

        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmDib);
        
        
        memset(pBits, 0, width * height * 4);

        
        DrawIconEx(hdcMem, 0, 0, hIcon, width, height, 0, NULL, DI_NORMAL);
        
        SelectObject(hdcMem, hbmOld);
        
        
        std::vector<unsigned char> rgba(width * height * 4);
        unsigned char* bgra = (unsigned char*)pBits;
        for (int i = 0; i < width * height; ++i) {
            rgba[i * 4 + 0] = bgra[i * 4 + 2]; 
            rgba[i * 4 + 1] = bgra[i * 4 + 1]; 
            rgba[i * 4 + 2] = bgra[i * 4 + 0]; 
            rgba[i * 4 + 3] = bgra[i * 4 + 3]; 
        }

        DeleteObject(hbmDib);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);

        
        D3D11_TEXTURE2D_DESC desc = {0};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = rgba.data();
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;

        ID3D11Texture2D* pTexture = nullptr;
        HRESULT hr = s_Device->CreateTexture2D(&desc, &subResource, &pTexture);
        if (FAILED(hr) || !pTexture) return nullptr;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;

        ID3D11ShaderResourceView* pSRV = nullptr;
        hr = s_Device->CreateShaderResourceView(pTexture, &srvDesc, &pSRV);
        pTexture->Release();

        return pSRV;
    }
}
