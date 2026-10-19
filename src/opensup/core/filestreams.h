// Copyright (C) 2024-2026 Lanzoone30
// SPDX-License-Identifier: GPL-3.0-or-later
//
// OpenSUP - PGS Encoder
//
// Adapted from SUPer by cubicibo (https://github.com/cubicibo/SUPer), GPL-3.0-or-later.

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <functional>

#include "opensup/core/segments.h"
#include "opensup/common/bdvideo.h"
#include "opensup/common/timecode.h"

namespace opensup {
namespace core {

/// One image inside a BDN event, with its declared box (X/Y/Width/Height).
struct bdn_graphic_t {
    int x = 0, y = 0, width = 0, height = 0;
    std::string path;
};

/// One subtitle event from the BDN XML (timing, geometry, image path).
class bdn_xml_event_c {
public:
    bdn_xml_event_c() = default;
    bdn_xml_event_c(common::tc_t tc_in, common::tc_t tc_out,
                     int x, int y, int width, int height,
                     std::string image_path,
                     bool forced = false);

    [[nodiscard]] double tc_in() const noexcept { return m_tc_in.to_pts(); }
    [[nodiscard]] double tc_out() const noexcept { return m_tc_out.to_pts(); }
    [[nodiscard]] const common::tc_t& in_tc() const noexcept { return m_tc_in; }
    [[nodiscard]] const common::tc_t& out_tc() const noexcept { return m_tc_out; }
    [[nodiscard]] int64_t frame_in() const noexcept { return m_tc_in.frames(); }
    [[nodiscard]] int64_t frame_out() const noexcept { return m_tc_out.frames(); }
    [[nodiscard]] int x() const noexcept { return m_x; }
    [[nodiscard]] int y() const noexcept { return m_y; }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }
    [[nodiscard]] bool forced() const noexcept { return m_forced; }
    [[nodiscard]] const std::string& image_path() const noexcept { return m_image_path; }

    void set_tc_in(common::tc_t tc) noexcept { m_tc_in = tc; }
    void set_tc_out(common::tc_t tc) noexcept { m_tc_out = tc; }

    /// Graphics of a multi-`<Graphic>` event. The event box is their union and
    /// load_image() composes the canvas; empty for the single-Graphic fast path.
    void set_graphics(std::vector<bdn_graphic_t> graphics) noexcept {
        m_graphics = std::move(graphics);
    }
    [[nodiscard]] const std::vector<bdn_graphic_t>& graphics() const noexcept {
        return m_graphics;
    }

    [[nodiscard]] std::vector<uint8_t> load_image() const;
    void unload() const noexcept;

private:
    common::tc_t m_tc_in, m_tc_out;
    int m_x = 0, m_y = 0, m_width = 0, m_height = 0;
    bool m_forced = false;
    std::string m_image_path;
    std::vector<bdn_graphic_t> m_graphics;
    mutable std::vector<uint8_t> m_cached_image;
};

/**
 * @brief Parsed BDN XML: events, fps, canvas size and image folder.
 */
class bdn_xml_c {
public:
    bdn_xml_c() = default;
    /// Parse a BDN XML file; throws parse_error_x on malformed input.
    bool parse(const std::string& filepath, bool ignore_resolution = false);

    [[nodiscard]] const std::vector<bdn_xml_event_c>& events() const noexcept { return m_events; }
    [[nodiscard]] common::fps_e fps() const noexcept { return m_fps; }
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }
    [[nodiscard]] const std::string& folder() const noexcept { return m_folder; }

    /// Group events into epochs; dt_split = max gap between events in one epoch.
    std::vector<std::vector<bdn_xml_event_c>> groups(double join_tol) const;

private:
    common::fps_e m_fps;
    int m_width = 0, m_height = 0;
    std::string m_folder;
    std::vector<bdn_xml_event_c> m_events;
    bool m_dropframe = false;
};

/// Reader/writer for .sup and .pes subtitle streams.
class sup_file_c {
public:
    explicit sup_file_c(const std::string& filepath);

    /// Parse all PGS segments from the stream.
    std::vector<std::shared_ptr<pg_segment_c>> read_segments();
    /// Parse segments and regroup them into display sets.
    std::vector<display_set_t> read_displaysets();
    /// Detect frame rate from segment timing metadata.
    common::fps_e get_fps();
    /// Canvas resolution (width, height) as declared by the stream.
    std::pair<int, int> get_video_format();

    /// Serialize segments to a .sup file.
    static void write_sup(const std::string& path,
                           const std::vector<std::shared_ptr<pg_segment_c>>& segments);

    /// Serialize segments to a .pes container (Blu-ray MUX ready).
    static void write_pes_mui(const std::string& pes_path,
                               const std::string& mui_path,
                               const std::vector<std::shared_ptr<pg_segment_c>>& segments);

private:
    std::string m_filepath;
};

std::vector<bdn_xml_event_c> remove_dupes(std::vector<bdn_xml_event_c>& events);
std::pair<std::vector<bdn_xml_event_c>, std::vector<bool>>
add_periodic_refreshes(const std::vector<bdn_xml_event_c>& events, common::fps_e fps, double period);

}
}
