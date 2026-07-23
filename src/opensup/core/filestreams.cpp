#include "opensup/pch.h"
#include "opensup/core/filestreams.h"
#include "opensup/core/segments.h"
#include "opensup/common/logger.h"

#include <pugixml.hpp>
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

// ── BDN XML Event ──
bdn_xml_event_c::bdn_xml_event_c(double tc_in_sec, double tc_out_sec,
                                   int x_, int y_, int width_, int height_,
                                   std::string image_path_,
                                   bool forced_, double /*fps*/)
    : m_tc_in(tc_in_sec), m_tc_out(tc_out_sec)
    , m_x(x_), m_y(y_), m_width(width_), m_height(height_)
    , m_forced(forced_), m_image_path(std::move(image_path_))
{}

std::vector<uint8_t>
bdn_xml_event_c::load_image() const
{
    if (!m_cached_image.empty())
        return m_cached_image;

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
bdn_xml_event_c::unload() noexcept
{
    m_cached_image.clear();
    m_cached_image.shrink_to_fit();
}

// ── BDN XML ──
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
    m_fps = common::fps_e::from_double(std::stod(fps_str));

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

    // Validate Blu-ray standard resolution
    if (!is_standard_resolution(m_width, m_height)) {
        auto msg = "Non-standard Blu-ray resolution " +
                   std::to_string(m_width) + "x" + std::to_string(m_height) +
                   ". Use --ignore-resolution or the checkbox in the GUI.";
        if (!ignore_resolution) {
            logger_c::instance().error(msg);
            return false;
        }
        logger_c::instance().warn(
            "Non-standard resolution " + std::to_string(m_width) + "x" +
            std::to_string(m_height) + ", continuing anyway.");
    }

    // Determine folder
    auto path = std::filesystem::path(filepath);
    m_folder = path.parent_path().string();

    // Parse events
    auto events_node = root.child("Events");
    if (!events_node) return false;

    for (auto event_node : events_node.children("Event")) {
        auto in_tc = event_node.attribute("InTC").as_string();
        auto out_tc = event_node.attribute("OutTC").as_string();
        auto forced_attr = event_node.attribute("Forced").as_string();
        bool forced = (std::string(forced_attr) == "true");

        auto graphic = event_node.child("Graphic");
        if (!graphic) continue;

        // ponytail: use first Graphic's box, multi-Graphic events deferred
        int gx = graphic.attribute("X").as_int(0);
        int gy = graphic.attribute("Y").as_int(0);
        int gw = graphic.attribute("Width").as_int(0);
        int gh = graphic.attribute("Height").as_int(0);
        auto gfx_file = graphic.child_value();

        auto img_path = std::filesystem::path(m_folder) / gfx_file;

        // Parse TC strings → seconds
        auto tc_to_sec = [this](const std::string& tc) -> double {
            // ponytail: HH:MM:SS:FF format
            int h = 0, m = 0, s = 0, f = 0;
            std::sscanf(tc.c_str(), "%d:%d:%d:%d", &h, &m, &s, &f);
            return static_cast<double>(h * 3600 + m * 60 + s) +
                   static_cast<double>(f) / m_fps.to_double();
        };

        double tc_in = tc_to_sec(in_tc);
        double tc_out = tc_to_sec(out_tc);

        if (tc_in >= tc_out) continue;

        bdn_xml_event_c ev(tc_in, tc_out, gx, gy, gw, gh,
                            img_path.string(), forced, m_fps.to_double());
        m_events.push_back(std::move(ev));
    }

    std::sort(m_events.begin(), m_events.end(),
              [](const auto& a, const auto& b) { return a.tc_in() < b.tc_in(); });

    logger_c::instance().log(common::log_level_e::hdebug, "Parsed " + std::to_string(m_events.size()) + " events from BDN XML");
    return true;
}

std::vector<std::vector<bdn_xml_event_c>>
bdn_xml_c::groups(double dt_split) const
{
    std::vector<std::vector<bdn_xml_event_c>> result;
    std::vector<bdn_xml_event_c> current;

    for (auto& ev : m_events) {
        if (current.empty()) {
            current.push_back(ev);
            continue;
        }
        double td = ev.tc_in() - current.back().tc_out();
        if (td >= 0 && td < dt_split) {
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

// ── SUP File ──
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

// ── Event helpers ──
std::vector<bdn_xml_event_c>
remove_dupes(std::vector<bdn_xml_event_c>& events)
{
    std::vector<bdn_xml_event_c> result;
    if (events.empty()) return result;

    result.push_back(events[0]);
    for (size_t i = 1; i < events.size() - 1; i++) {
        auto& prev = events[i];
        auto& next = events[i + 1];
        bool same_pos = (next.x() == prev.x() && next.y() == prev.y());
        bool same_shape = (next.width() == prev.width() && next.height() == prev.height());

        if (same_pos && same_shape) {
            // ponytail: simple dupe check without full pixel comparison
            result.back().set_tc_out(next.tc_out());
        } else {
            result.push_back(next);
        }
    }
    if (events.size() >= 2)
        result.push_back(events.back());

    auto& first = events.front();
    auto& last = events.back();
    auto& rfirst = result.front();
    auto& rlast = result.back();
    if (std::abs(rfirst.tc_in() - first.tc_in()) > 0.001 ||
        std::abs(rlast.tc_out() - last.tc_out()) > 0.001) {
        logger_c::instance().log(common::log_level_e::hdebug, "Deduplication range mismatch");
    }

    logger_c::instance().log(common::log_level_e::hdebug, "Removed " + std::to_string(events.size() - result.size()) +
                               " duplicate event(s).");
    return result;
}

std::pair<std::vector<bdn_xml_event_c>, std::vector<bool>>
add_periodic_refreshes(const std::vector<bdn_xml_event_c>& events, double fps, double period)
{
    std::vector<bdn_xml_event_c> new_events;
    std::vector<bool> redraw_flags;

    int frame_period = static_cast<int>(std::round(period * fps));
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

        double frames_dur = (ev.tc_out() - ev.tc_in()) * fps;
        int count = static_cast<int>(frames_dur / frame_period) - 1;

        if (count >= 1) {
            auto final_tc_out = ev.tc_out();
            double frame_dur = 1.0 / fps;
            double prev_tc_out = ev.tc_in() + frame_dur * frame_period;

            new_events.back().set_tc_out(prev_tc_out);

            for (int k = 0; k < count; k++) {
                bdn_xml_event_c copy = ev;
                copy.set_tc_in(prev_tc_out);
                prev_tc_out += frame_dur * frame_period;
                copy.set_tc_out(prev_tc_out);
                new_events.push_back(std::move(copy));
                redraw_flags.push_back(true);
            }
            new_events.back().set_tc_out(final_tc_out);
        }
    }
    return {new_events, redraw_flags};
}

} // namespace core
} // namespace opensup
