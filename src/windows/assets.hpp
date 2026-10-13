#pragma once
#include "core/package.hpp"
#include "core/surface.hpp"
#include <memory>
namespace pet {
struct Assets {
    Package package;
    std::shared_ptr<Surface> atlas;
    bool decoded = false;
    static Assets load(const std::filesystem::path& manifest, const Assets* previous = nullptr);
};
}
