#pragma once

namespace cereka {

class ITexture {
public:
    virtual ~ITexture() = default;
    virtual float Width() const = 0;
    virtual float Height() const = 0;
};

} // namespace cereka
