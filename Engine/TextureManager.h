#pragma once
#include "Texture.h"
#include <memory>
#include <filesystem>

// Rendering-thread API. The last owner releases the texture's GPU resources.
namespace TextureManager {
    std::shared_ptr<Texture> Load(const std::filesystem::path& path);
}
