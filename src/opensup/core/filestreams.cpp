// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#include "opensup/pch.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/segments.h"
#include "opensup/common/logger.h"

#include <pugixml.hpp>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#include "stb_image.h"
#pragma GCC diagnostic pop

namespace opensup {
namespace core {

using common::logger_c;

bdn_xml_event_c::bdn_xml_event_c(common::tc_t tc_in, common::tc_t tc_out,
                                   int x_, int y_, int width_, int height_,
                                   std::string image_path_,
                                   bool forced_)
    : m_tc_in(tc_in), m_tc_out(tc_out)
    , m_x(x_), m_y(y_), m_width(width_), m_height(height_)
    , m_forced(forced_), m_image_path(std::move(image_path_))
{}

std::vector<uint8_t>
bdn_xml_event_c::load_image() const
{
    if (!m_cached_image.empty())
        return m_cached_image;

    if (m_graphics.size() > 1) {
        // Union canvas: paste every graphic at its offset relative to the
        // composed box (oracle filestreams.py:306-317).
        m_cached_image.assign(
            static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4, 0);
        for (const auto& g : m_graphics) {
            int gw = 0, gh = 0, gc = 0;
            unsigned char* img = stbi_load(g.path.c_str(), &gw, &gh, &gc, 4);
            if (!img) {
                logger_c::instance().error("Failed to load image: " + g.path);
                continue;
            }
            const int ox = g.x - m_x, oy = g.y - m_y;
            const size_t sw = static_cast<size_t>(gw);
            const size_t cw = static_cast<size_t>(m_width);
            for (int y = 0; y < gh; ++y) {
                const int dy = oy + y;
                if (dy < 0 || dy >= m_height) continue;
                for (int x = 0; x < gw; ++x) {
                    const int dx = ox + x;
                    if (dx < 0 || dx >= m_width) continue;
                    const unsigned char* s =
                        img + (static_cast<size_t>(y) * sw +
                               static_cast<size_t>(x)) * 4;
                    unsigned char* dt = m_cached_image.data() +
                        (static_cast<size_t>(dy) * cw +
                         static_cast<size_t>(dx)) * 4;
                    dt[0] = s[0];
                    dt[1] = s[1];
                    dt[2] = s[2];
                    dt[3] = s[3];
                }
            }
            stbi_image_free(img);
        }
        return m_cached_image;
    }

    int w = 0, h = 0, channels = 0;
    unsigned char* img = stbi_load(m_image_path.c_str(), &w, &h, &channels, 4);
    if (!img) {
        logger_c::instance().error("Failed to load image: " + m_image_path);
        return {};
    }
    auto iw = static_cast<size_t>(w), ih = static_cast<size_t>(h);
    m_cached_image.assign(img, img + iw * ih * 4);
    stbi_image_free(img);
    return m_cached_image;
}

void
bdn_xml_event_c::unload() const noexcept
{
    m_cached_image.clear();
    m_cached_image.shrink_to_fit();
}

static bool
is_standard_resolution(int w, int h)
{
    return (w == 1920 && h == 1080) ||
           (w == 1280 && h == 720)  ||
           (w == 720  && h == 576)  ||
           (w == 720  && h == 480);
}

bool
bdn_xml_c::parse(const std::string& filepath, bool ignore_resolution)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());

    if (!result) {
        logger_c::instance().error("BDN XML parse error: " + std::string(result.description()));
        return false;
    }

    auto root = doc.document_element();
    if (!root) return false;

    auto header = root.child("Description").child("Format");
    if (!header) return false;

    auto fps_str = header.attribute("FrameRate").as_string();
    m_dropframe = std::string(header.attribute("DropFrame").as_string()) == "true";
    // FrameRate may be "24", "24000/1001", "25/1", etc.
    // Untrusted attribute: reject the file when it does not parse cleanly.
    double fps_val;
    auto fps_s = std::string(fps_str);
    auto slash = fps_s.find('/');
    try {
        if (slash != std::string::npos) {
            double num = std::stod(fps_s.substr(0, slash));
            double den = std::stod(fps_s.substr(slash + 1));
            fps_val = num / den;
        } else {
            fps_val = std::stod(fps_s);
        }
        m_fps = common::fps_e::from_double(fps_val);
    } catch (const std::exception&) {
        return false;  // malformed FrameRate: cannot encode without a valid rate
    }

    // Parse video format (WIDTHxHEIGHT or just HEIGHT)
    auto vf_str = header.attribute("VideoFormat").as_string();
    try {
        auto x_pos = std::string(vf_str).find('x');
        if (x_pos != std::string::npos) {
            m_width = std::stoi(std::string(vf_str).substr(0, x_pos));
            m_height = std::stoi(std::string(vf_str).substr(x_pos + 1));
        } else {
            int h = std::stoi(vf_str);
            m_width = (h == 1080) ? 1920 : (h == 720) ? 1280 : 720;
            m_height = h;
        }
    } catch (...) {
        m_width = 1920;
        m_height = 1080;
    }

    // Untrusted size: an arbitrary resolution is still a positive one.
    if (m_width <= 0 || m_height <= 0) {
        logger_c::instance().error(
            "Invalid video resolution " + std::to_string(m_width) + "x" +
            std::to_string(m_height) + ".");
        return false;
    }

    // Validate Blu-ray standard resolution
    if (!is_standard_resolution(m_width, m_height)) {
        if (!ignore_resolution) {
            logger_c::instance().error(
                "Non-standard Blu-ray resolution " +
                std::to_string(m_width) + "x" + std::to_string(m_height) + ".");
            return false;
        }
        // Emitted once per file, before the event loop.
        logger_c::instance().warn(
            "Non-standard resolution " + std::to_string(m_width) + "x" +
            std::to_string(m_height) + ", continuing anyway.");
    }

    auto path = std::filesystem::path(filepath);
    m_folder = path.parent_path().string();

    auto events_node = root.child("Events");
    if (!events_node) return false;

    for (auto event_node : events_node.children("Event")) {
        auto in_tc = event_node.attribute("InTC").as_string();
        auto out_tc = event_node.attribute("OutTC").as_string();
        auto forced_attr = event_node.attribute("Forced").as_string();
        bool forced = (std::string(forced_attr) == "true");

        auto tc_from_string = [this](const std::string& tc) -> common::tc_t {
            int h = 0, m = 0, s = 0, f = 0;
            std::sscanf(tc.c_str(), "%d:%d:%d:%d", &h, &m, &s, &f);
            return common::tc_t::from_hmsf(h, m, s, f, m_fps);
        };

        // N <Graphic> = one event with a union canvas (oracle filestreams.py:224-239).
        std::vector<bdn_graphic_t> graphics;
        for (auto graphic : event_node.children("Graphic")) {
            bdn_graphic_t g;
            g.x = graphic.attribute("X").as_int(0);
            g.y = graphic.attribute("Y").as_int(0);
            g.width = graphic.attribute("Width").as_int(0);
            g.height = graphic.attribute("Height").as_int(0);
            g.path = (std::filesystem::path(m_folder) /
                      graphic.child_value()).string();
            graphics.push_back(std::move(g));
        }
        if (graphics.empty()) continue;

        // US2: on a multi-<Graphic> canvas every declared box must match the
        // real PNG and the graphics must not overlap (oracle asserts, 306-317).
        if (graphics.size() > 1) {
            for (const auto& g : graphics) {
                int rw = 0, rh = 0, rc = 0;
                if (!stbi_info(g.path.c_str(), &rw, &rh, &rc)) {
                    logger_c::instance().error("Failed to read image: " + g.path);
                    return false;
                }
                if (rw != g.width || rh != g.height) {
                    logger_c::instance().error(
                        "Declared <Graphic> " + std::to_string(g.width) + "x" +
                        std::to_string(g.height) + " but " + g.path + " is " +
                        std::to_string(rw) + "x" + std::to_string(rh));
                    return false;
                }
            }
            for (size_t a = 0; a < graphics.size(); ++a) {
                for (size_t b = a + 1; b < graphics.size(); ++b) {
                    const auto& p = graphics[a];
                    const auto& q = graphics[b];
                    const int ix0 = std::max(p.x, q.x), iy0 = std::max(p.y, q.y);
                    const int ix1 = std::min(p.x + p.width, q.x + q.width);
                    const int iy1 = std::min(p.y + p.height, q.y + q.height);
                    if (ix1 > ix0 && iy1 > iy0) {
                        logger_c::instance().error(
                            "Overlapping <Graphic>s in event " +
                            std::string(in_tc));
                        return false;
                    }
                }
            }
        }

        int ux = graphics[0].x, uy = graphics[0].y;
        int ux2 = ux + graphics[0].width, uy2 = uy + graphics[0].height;
        for (const auto& g : graphics) {
            ux = std::min(ux, g.x);
            uy = std::min(uy, g.y);
            ux2 = std::max(ux2, g.x + g.width);
            uy2 = std::max(uy2, g.y + g.height);
        }

        common::tc_t tc_in = tc_from_string(in_tc);
        common::tc_t tc_out = tc_from_string(out_tc);

        if (tc_in.frames() >= tc_out.frames()) continue;

        bdn_xml_event_c ev(tc_in, tc_out, ux, uy, ux2 - ux, uy2 - uy,
                            graphics[0].path, forced);
        if (graphics.size() > 1)
            ev.set_graphics(std::move(graphics));
        m_events.push_back(std::move(ev));
    }

    std::sort(m_events.begin(), m_events.end(),
              [](const auto& a, const auto& b) { return a.tc_in() < b.tc_in(); });

    logger_c::instance().log(common::log_level_e::hdebug, "Parsed " + std::to_string(m_events.size()) + " events from BDN XML");
    return true;
}

std::vector<std::vector<bdn_xml_event_c>>
bdn_xml_c::groups(double join_tol) const
{
    std::vector<std::vector<bdn_xml_event_c>> result;
    std::vector<bdn_xml_event_c> current;

    for (auto& ev : m_events) {
        if (current.empty()) {
            current.push_back(ev);
            continue;
        }
        double td = ev.tc_in() - current.back().tc_out();
        // Events join the current epoch while their gap is shorter than the split
        // threshold (proportional to the full-screen decode cost).
        if (td < join_tol) {
            current.push_back(ev);
        } else {
            result.push_back(std::move(current));
            current.clear();
            current.push_back(ev);
        }
    }
    if (!current.empty())
        result.push_back(std::move(current));
    return result;
}

sup_file_c::sup_file_c(const std::string& filepath)
    : m_filepath(filepath)
{
    if (!std::filesystem::exists(filepath))
        throw std::runtime_error("SUP file does not exist: " + filepath);
}

std::vector<std::shared_ptr<pg_segment_c>>
sup_file_c::read_segments()
{
    std::ifstream f(m_filepath, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open SUP file");

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
    return display_set_t::from_bytes(buffer).segments;
}

std::vector<display_set_t>
sup_file_c::read_displaysets()
{
    auto segs = read_segments();
    std::vector<display_set_t> dss;
    std::vector<std::shared_ptr<pg_segment_c>> current;

    for (auto& seg : segs) {
        current.push_back(seg);
        if (seg->type() == segment_type_e::end) {
            dss.emplace_back(std::move(current));
            current.clear();
        }
    }
    return dss;
}

common::fps_e
sup_file_c::get_fps()
{
    auto segs = read_segments();
    if (segs.empty() || segs[0]->type() != segment_type_e::pcs)
        return common::fps_e(common::fps_e::film);
    auto& pcs = dynamic_cast<pcs_c&>(*segs[0]);
    return common::fps_e::from_pcsfps(pcs.fps());
}

std::pair<int, int>
sup_file_c::get_video_format()
{
    auto segs = read_segments();
    if (segs.empty() || segs[0]->type() != segment_type_e::pcs)
        return {1920, 1080};
    auto& pcs = dynamic_cast<pcs_c&>(*segs[0]);
    return {pcs.video_width(), pcs.video_height()};
}

void
sup_file_c::write_sup(const std::string& path,
                       const std::vector<std::shared_ptr<pg_segment_c>>& segments)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot write SUP file: " + path);

    for (auto& seg : segments) {
        auto bytes = seg->to_bytes();
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
}

// PES strips 13-byte PG header, MUI stores timestamps in 54MHz custom format
static constexpr uint64_t MUI_TS_OFFSET = 54'000'000;

void
sup_file_c::write_pes_mui(const std::string& pes_path,
                           const std::string& mui_path,
                           const std::vector<std::shared_ptr<pg_segment_c>>& segments)
{
    std::ofstream fpes(pes_path, std::ios::binary);
    if (!fpes) throw std::runtime_error("Cannot write PES file: " + pes_path);

    std::ofstream fmui(mui_path, std::ios::binary);
    if (!fmui) throw std::runtime_error("Cannot write MUI file: " + mui_path);

    // MUI header: 0x00000003 (GRAPHICS type)
    uint8_t mui_hdr[4] = {0x00, 0x00, 0x00, 0x03};
    fmui.write(reinterpret_cast<const char*>(mui_hdr), 4);

    for (auto& seg : segments) {
        auto bytes = seg->to_bytes();

        // PES: write segment type + length + payload (skip PG magic + PTS/DTS)
        // bytes[0:1] = "PG", bytes[2:5] = PTS, bytes[6:9] = DTS = 10 bytes to skip
        // bytes[10] = type, bytes[11:12] = length, bytes[13:] = payload
        auto pes_start = static_cast<std::streamsize>(PGS_HEADER_LEN - 3);
        auto pes_len = static_cast<std::streamsize>(bytes.size()) - pes_start;
        fpes.write(reinterpret_cast<const char*>(bytes.data() + pes_start), pes_len);

        // MUI: convert PTS/DTS to 54MHz timestamps
        uint32_t raw_dts = seg->tdts();
        uint32_t raw_pts = seg->tpts();
        uint64_t mui_dts = static_cast<uint64_t>(raw_dts) + MUI_TS_OFFSET;
        uint64_t mui_pts = static_cast<uint64_t>(raw_pts) + MUI_TS_OFFSET;
        // from_mui rebuilds a 39-bit PTS then shifts it >>6: emit PTS<<6.
        uint64_t mui_pts_field = (mui_pts << 6) & 0x7F'FFFFFFFFULL;

        uint8_t mui_entry[14];
        // Entry header: segment type + block length (length + 3 for MUI prefix)
        mui_entry[0] = bytes[10];  // segment type
        uint32_t block_len = static_cast<uint32_t>(bytes.size() - PGS_HEADER_LEN + 3);
        mui_entry[1] = static_cast<uint8_t>((block_len >> 24) & 0xFF);
        mui_entry[2] = static_cast<uint8_t>((block_len >> 16) & 0xFF);
        mui_entry[3] = static_cast<uint8_t>((block_len >> 8) & 0xFF);
        mui_entry[4] = static_cast<uint8_t>(block_len & 0xFF);

        // 9-byte MUI timestamp: DTS (33 bits) + PTS (39 bits)
        mui_entry[5]  = static_cast<uint8_t>((mui_dts >> 25) & 0xFF);  // DTS[32:25]
        mui_entry[6]  = static_cast<uint8_t>((mui_dts >> 17) & 0xFF);  // DTS[24:17]
        mui_entry[7]  = static_cast<uint8_t>((mui_dts >> 9) & 0xFF);   // DTS[16:9]
        mui_entry[8]  = static_cast<uint8_t>((mui_dts >> 1) & 0xFF);   // DTS[8:1]
        mui_entry[9]  = static_cast<uint8_t>((mui_dts & 1) << 7);      // DTS[0] in MSB
        mui_entry[9] |= static_cast<uint8_t>((mui_pts_field >> 32) & 0x7F);  // PTS[38:32]
        mui_entry[10] = static_cast<uint8_t>((mui_pts_field >> 24) & 0xFF);  // PTS[31:24]
        mui_entry[11] = static_cast<uint8_t>((mui_pts_field >> 16) & 0xFF);  // PTS[23:16]
        mui_entry[12] = static_cast<uint8_t>((mui_pts_field >> 8) & 0xFF);   // PTS[15:8]
        mui_entry[13] = static_cast<uint8_t>(mui_pts_field & 0xFF);          // PTS[7:0]

        fmui.write(reinterpret_cast<const char*>(mui_entry), 14);
    }

    // MUI tail: 0xFF + 13 zero bytes
    uint8_t mui_tail[14] = {0xFF, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0};
    fmui.write(reinterpret_cast<const char*>(mui_tail), 14);
}

std::vector<bdn_xml_event_c>
remove_dupes(std::vector<bdn_xml_event_c>& events)
{
    if (events.empty()) return {};

    std::vector<bdn_xml_event_c> result;
    result.push_back(events[0]);
    for (size_t i = 0; i + 1 < events.size(); i++) {
        const auto& cur = events[i];
        const auto& next = events[i + 1];
        const bool same_geom =
            next.x() == cur.x() && next.y() == cur.y() &&
            next.width() == cur.width() && next.height() == cur.height();
        const bool same_image = same_geom && next.load_image() == cur.load_image();
        // Only a contiguous, identical successor is folded into the previous span.
        const bool contiguous = result.back().out_tc().frames() == next.in_tc().frames();
        if (!same_image || !contiguous)
            result.push_back(next);
        else
            result.back().set_tc_out(next.out_tc());
    }

    logger_c::instance().log(common::log_level_e::hdebug,
        "Removed " + std::to_string(events.size() - result.size()) + " duplicate event(s).");
    return result;
}

std::pair<std::vector<bdn_xml_event_c>, std::vector<bool>>
add_periodic_refreshes(const std::vector<bdn_xml_event_c>& events, common::fps_e fps, double period)
{
    std::vector<bdn_xml_event_c> new_events;
    std::vector<bool> redraw_flags;

    int frame_period = static_cast<int>(std::round(period * fps.to_double()));
    if (frame_period < 1) {
        for (auto& ev : events) {
            new_events.push_back(ev);
            redraw_flags.push_back(false);
        }
        return {new_events, redraw_flags};
    }

    for (auto& ev : events) {
        new_events.push_back(ev);
        redraw_flags.push_back(false);

        // Integer frame arithmetic: the duration must not round through seconds.
        int64_t frames_dur = ev.frame_out() - ev.frame_in();
        int count = static_cast<int>(frames_dur / frame_period) - 1;

        if (count >= 1) {
            common::tc_t final_tc_out = ev.out_tc();
            int64_t prev_frame = ev.frame_in() + frame_period;

            new_events.back().set_tc_out(common::tc_t::from_frames(prev_frame, fps));

            for (int k = 0; k < count; k++) {
                bdn_xml_event_c copy = ev;
                copy.set_tc_in(common::tc_t::from_frames(prev_frame, fps));
                prev_frame += frame_period;
                copy.set_tc_out(common::tc_t::from_frames(prev_frame, fps));
                new_events.push_back(std::move(copy));
                redraw_flags.push_back(true);
            }
            new_events.back().set_tc_out(final_tc_out);
        }
    }
    return {new_events, redraw_flags};
}

}
}
