#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>

#include "opensup/core/segments.h"
#include "opensup/common/bdvideo.h"

namespace opensup {
namespace core {

// ── BDN XML Event ──
class bdn_xml_event_c {
public:
    bdn_xml_event_c() = default;
    bdn_xml_event_c(double tc_in_sec, double tc_out_sec,
                     int x, int y, int width, int height,
                     std::string image_path,
                     bool forced = false,
                     double fps = 24.0);

    [[nodiscard]] double tc_in() const noexcept { return m_tc_in; }
    [[nodiscard]] double tc_out() const noexcept { return m_tc_out; }
    [[nodiscard]] int x() const noexcept { return m_x; }
    [[nodiscard]] int y() const noexcept { return m_y; }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }
    [[nodiscard]] bool forced() const noexcept { return m_forced; }
    [[nodiscard]] const std::string& image_path() const noexcept { return m_image_path; }

    void set_tc_in(double tc) noexcept { m_tc_in = tc; }
    void set_tc_out(double tc) noexcept { m_tc_out = tc; }

    [[nodiscard]] std::vector<uint8_t> load_image() const;
    void unload() noexcept;

private:
    double m_tc_in = 0.0;
    double m_tc_out = 0.0;
    int m_x = 0, m_y = 0, m_width = 0, m_height = 0;
    bool m_forced = false;
    std::string m_image_path;
    mutable std::vector<uint8_t> m_cached_image;
};

// ── BDN XML ──
class bdn_xml_c {
public:
    bdn_xml_c() = default;
    bool parse(const std::string& filepath, bool ignore_resolution = false);

    [[nodiscard]] const std::vector<bdn_xml_event_c>& events() const noexcept { return m_events; }
    [[nodiscard]] common::fps_e fps() const noexcept { return m_fps; }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }
    [[nodiscard]] const std::string& folder() const noexcept { return m_folder; }

    std::vector<std::vector<bdn_xml_event_c>> groups(double dt_split) const;

private:
    common::fps_e m_fps;
    int m_width = 0, m_height = 0;
    std::string m_folder;
    std::vector<bdn_xml_event_c> m_events;
    bool m_dropframe = false;
};

// ── SUP File ──
class sup_file_c {
public:
    explicit sup_file_c(const std::string& filepath);

    std::vector<std::shared_ptr<pg_segment_c>> read_segments();
    std::vector<display_set_t> read_displaysets();
    common::fps_e get_fps();
    std::pair<int, int> get_video_format();

    static void write_sup(const std::string& path,
                           const std::vector<std::shared_ptr<pg_segment_c>>& segments);

    static void write_pes_mui(const std::string& pes_path,
                               const std::string& mui_path,
                               const std::vector<std::shared_ptr<pg_segment_c>>& segments);

private:
    std::string m_filepath;
};

// ── Event helpers ──
std::vector<bdn_xml_event_c> remove_dupes(std::vector<bdn_xml_event_c>& events);
std::pair<std::vector<bdn_xml_event_c>, std::vector<bool>>
add_periodic_refreshes(const std::vector<bdn_xml_event_c>& events, double fps, double period);

} // namespace core
} // namespace opensup
