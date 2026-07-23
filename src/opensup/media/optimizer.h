#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <utility>

#include "opensup/media/palette.h"

namespace opensup {
namespace media {

struct quantize_result_t {
    media::palette_t palette;
    std::vector<uint8_t> indexed;  // palette index per pixel
};

class quantizer_base_c {
public:
    virtual ~quantizer_base_c() = default;
    virtual quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                                       int width, int height, int max_colors) = 0;
    virtual std::string name() const = 0;
};

class libimagequant_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors) override;
    std::string name() const override { return "libimagequant"; }
};

class hextree_t : public quantizer_base_c {
public:
    quantize_result_t quantize(const std::vector<uint8_t>& rgba,
                               int width, int height, int max_colors) override;
    std::string name() const override { return "HexTree"; }
};

class optimiser_c {
public:
    static std::vector<std::unique_ptr<quantizer_base_c>> get_available();
};

} // namespace media
} // namespace opensup
