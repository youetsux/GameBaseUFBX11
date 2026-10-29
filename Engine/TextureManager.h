#pragma once
#include "Texture.h"
#include <memory>
#include <filesystem>

// 描画スレッドから使用。読み込んだ画像はClearまで共有して保持する。
namespace TextureManager
{
    void Clear();
    std::shared_ptr<Texture> Load(const std::filesystem::path &path);
} // namespace TextureManager
