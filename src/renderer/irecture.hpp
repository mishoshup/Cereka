#pragma once

struct SDL_Texture;  // forward declare for RawTexture escape hatch

namespace cereka {

class ITexture {
public:
    virtual ~ITexture() = default;
    virtual float Width() const = 0;
    virtual float Height() const = 0;

    // Temporary escape hatch for existing SDL-based draw code during migration.
    // Plan 03-03 (UIManager extraction) removes all direct SDL calls from
    // draw code, making this unnecessary.
    [[deprecated("Use ITexture Width/Height + IRenderContext DrawTexture instead")]]
    virtual SDL_Texture *RawTexture() const = 0;
};

} // namespace cereka
