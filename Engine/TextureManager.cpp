#include "TextureManager.h"
#include "Direct3D.h"
#include <vector>

namespace {
    namespace fs = std::filesystem;

    struct CachedTexture {
        ID3D11Device* device = nullptr;
        std::wstring path;
        std::weak_ptr<Texture> texture;
    };

    // weak_ptrは画像を所有しない。材質などの最後の利用者が画像を解放する。
    // デバイスも所有せず、使用済みの項目は次のLoad時に取り除く。
    std::vector<CachedTexture> textureCache;

    void RemoveExpiredTextures() {
        size_t index = 0;
        while (index < textureCache.size()) {
            if (textureCache[index].texture.expired()) {
                textureCache.erase(textureCache.begin() + index);
                // 後ろの項目が詰まるため、削除したときはindexを進めない。
            } else {
                ++index;
            }
        }
    }

    std::shared_ptr<Texture> FindTexture(const std::wstring& path) {
        for (const CachedTexture& entry : textureCache) {
            if (entry.device != Direct3D::pDevice_) {
                continue;
            }
            if (entry.path != path) {
                continue;
            }
            return entry.texture.lock();
        }
        return nullptr;
    }
}

namespace TextureManager {
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
        std::wstring cachePath = fullPath.native();
        CharLowerBuffW(cachePath.data(), static_cast<DWORD>(cachePath.size()));

        RemoveExpiredTextures();
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
        entry.device = Direct3D::pDevice_;
        entry.path = cachePath;
        entry.texture = texture;
        textureCache.push_back(entry);
        return texture;
    }
}
