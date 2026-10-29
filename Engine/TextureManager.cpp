#include "TextureManager.h"
#include "Direct3D.h"
#include <vector>

namespace {


    struct CachedTexture {
        ID3D11Device* device = nullptr;
        std::wstring path;
        std::shared_ptr<Texture> texture;
    };

    // 読み込んだ画像は、Clear()が呼ばれるまで管理表で保持する。
    std::vector<CachedTexture> textureCache;

    // 大文字・小文字を区別せず、パスを比較する。
    bool IsSamePath(const std::wstring& first, const std::wstring& second) {
        return CompareStringOrdinal(
            first.c_str(), -1,
            second.c_str(), -1,
            TRUE
        ) == CSTR_EQUAL;
    }

    std::shared_ptr<Texture> FindTexture(const std::wstring& path) {
        for (const CachedTexture& entry : textureCache) {
            if (entry.device != Direct3D::pDevice_.Get()) {
                continue;
            }
            if (!IsSamePath(entry.path, path)) {
                continue;
            }
            return entry.texture;
        }
        return nullptr;
    }
}

namespace TextureManager {
    void Clear() {
        // 使用中の材質があれば、その材質のshared_ptrによって画像は存続する。
        textureCache.clear();
    }

    std::shared_ptr<Texture> Load(const std::filesystem::path& path) {
        if (Direct3D::pDevice_ == nullptr || path.empty()) {
            return nullptr;
        }

        std::error_code error;
        std::filesystem::path fullPath = std::filesystem::absolute(path, error);
        if (error) {
            return nullptr;
        }
        fullPath = fullPath.lexically_normal();

        // Windows上で、大文字小文字だけが異なるパスも同じ画像として扱う。
        const std::wstring cachePath = fullPath.native();



        std::shared_ptr<Texture> texture = FindTexture(cachePath);
        if (texture != nullptr) {
            return texture;
        }

        // 未登録の画像だけ、従来のTexture APIで読み込む。
        texture = std::make_shared<Texture>();
        const HRESULT result = texture->Load(fullPath.string());
        if (FAILED(result)) {
            return nullptr;
        }

        CachedTexture entry;
        entry.device = Direct3D::pDevice_.Get();
        entry.path = cachePath;
        entry.texture = texture;
        textureCache.push_back(entry);
        return texture;
    }
}

