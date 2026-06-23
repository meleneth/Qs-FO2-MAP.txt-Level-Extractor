#include <catch2/catch_test_macros.hpp>

#include "gui_session.h"

#include <filesystem>
#include <string>

namespace {

std::filesystem::path fixture_path(std::string_view name)
{
    return std::filesystem::path(TEST_MAPS_DIR) / std::filesystem::path(name);
}

qmap::GuiSession::MapSlot loaded_slot(const char* name, const char* path)
{
    qmap::GuiSession::MapSlot slot;
    slot.map_name = name;
    slot.file_path = path;
    slot.owned_data = {0};
    slot.elevations[0] = qmap::Range{0, 1};
    slot.elevations[2] = qmap::Range{0, 1};
    return slot;
}

} // namespace

TEST_CASE("GUI session updates loaded map labels without preserving stale output selections", "[gui]")
{
    qmap::GuiSession session;
    session.output_selection[1] = qmap::ElevationSource{qmap::MapSide::right, 2};
    session.output_labels[1] = "2:old.map";

    session.left = loaded_slot("source.txt", "C:/maps/source.txt");
    qmap::update_loaded_map_labels(session, qmap::MapSide::left);

    CHECK(session.left.heading == "source.txt");
    CHECK(session.left.labels[0] == "0:source.txt");
    CHECK(session.left.labels[1] == "empty");
    CHECK(session.left.labels[2] == "2:source.txt");
    CHECK_FALSE(session.output_selection[1].has_value());
    CHECK(session.output_labels[1] == "##1");
}

TEST_CASE("GUI session output selection uses explicit source coordinates", "[gui]")
{
    qmap::GuiSession session;
    const auto slot = loaded_slot("right.txt", "C:/maps/right.txt");

    qmap::select_output_elevation(session, 2, slot, "0:right.txt", qmap::MapSide::right, 0);

    REQUIRE(session.output_selection[2].has_value());
    CHECK(session.output_selection[2]->side == qmap::MapSide::right);
    CHECK(session.output_selection[2]->elevation.value == 0);
    CHECK(session.output_labels[2] == "0:right.txt");

    qmap::clear_output_elevation(session, 2);

    CHECK_FALSE(session.output_selection[2].has_value());
    CHECK(session.output_labels[2] == "##2");
}

TEST_CASE("GUI session header selection updates export path and export plan", "[gui]")
{
    qmap::GuiSession session;
    session.right = loaded_slot("right.txt", "C:/maps/right.txt");

    qmap::choose_output_header(session, qmap::MapSide::right);

    CHECK(session.header == 1);
    CHECK(session.middle_head == "right.txt##");
    CHECK(std::string{session.export_path} == "C:/maps/right.txt.Q.txt");

    const auto plan = qmap::make_text_export_plan(session);
    REQUIRE(plan.header_side.has_value());
    CHECK(*plan.header_side == qmap::MapSide::right);

    qmap::clear_output_header(session);

    CHECK(session.header == -1);
    CHECK(session.middle_head == "empty");
}

TEST_CASE("GUI session rejects mixed binary and text export", "[gui]")
{
    qmap::GuiSession session;
    session.left = loaded_slot("left.map", "C:/maps/left.map");
    session.left.map_type = qmap::MapFileKind::binary;
    session.right = loaded_slot("right.txt", "C:/maps/right.txt");
    session.right.map_type = qmap::MapFileKind::text;

    const auto action = qmap::prepare_export(session);

    CHECK(action == qmap::GuiExportAction::none);
    CHECK(session.open_error_popup);
    CHECK(session.current_error.find("can't mix .MAP and .TXT") != std::string::npos);
}

TEST_CASE("GUI session loads dropped text maps into the selected side", "[gui]")
{
    qmap::GuiSession session;
    session.drop_target = qmap::MapSide::left;

    REQUIRE(qmap::load_dropped_file(session, fixture_path("ARVILL2.txt")));

    CHECK(session.left.map_type == qmap::MapFileKind::text);
    CHECK_FALSE(session.left.owned_data.empty());
    REQUIRE(session.left.parsed_text.has_value());
    CHECK(session.left.parsed_text->scripts.size > 0);
    CHECK(session.left.parsed_text->objects.size > 0);
    CHECK(session.left.heading == "ARVILL2.txt");
    CHECK(session.left.labels[0] == "0:ARVILL2.txt");
    CHECK(session.left.labels[1] == "empty");
    CHECK_FALSE(session.drop_target.has_value());
}

TEST_CASE("GUI session reports unsupported dropped file types", "[gui]")
{
    qmap::GuiSession session;
    session.drop_target = qmap::MapSide::right;

    CHECK_FALSE(qmap::load_dropped_file(session, fixture_path("not-a-map.foo")));

    CHECK(session.right.owned_data.empty());
    CHECK(session.open_error_popup);
    CHECK(session.current_error.find("Wrong file type") != std::string::npos);
}
