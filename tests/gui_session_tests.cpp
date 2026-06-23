#include <catch2/catch_test_macros.hpp>

#include "gui_session.h"

namespace {

map_lvls loaded_map(const char* name, const char* path)
{
    static unsigned char data = 0;

    map_lvls map;
    map.map_name = const_cast<char*>(name);
    map.file_str = const_cast<char*>(path);
    map.data = &data;
    map.level[0] = reinterpret_cast<char*>(&data);
    map.level[2] = reinterpret_cast<char*>(&data);
    return map;
}

} // namespace

TEST_CASE("GUI session updates loaded map labels without preserving stale output selections", "[gui]")
{
    qmap::GuiSession session;
    session.output_selection[1] = qmap::ElevationSource{qmap::MapSide::right, 2};
    session.output_labels[1] = "2:old.map";

    const auto map = loaded_map("source.txt", "C:/maps/source.txt");
    qmap::update_loaded_map_labels(session, map, qmap::MapSide::left);

    CHECK(session.left_head == "source.txt");
    CHECK(session.left_labels[0] == "0:source.txt");
    CHECK(session.left_labels[1] == "empty");
    CHECK(session.left_labels[2] == "2:source.txt");
    CHECK_FALSE(session.output_selection[1].has_value());
    CHECK(session.output_labels[1] == "##1");
}

TEST_CASE("GUI session output selection uses explicit source coordinates", "[gui]")
{
    qmap::GuiSession session;
    const auto map = loaded_map("right.txt", "C:/maps/right.txt");

    qmap::select_output_elevation(session, 2, map, "0:right.txt", qmap::MapSide::right, 0);

    REQUIRE(session.output_selection[2].has_value());
    CHECK(session.output_selection[2]->side == qmap::MapSide::right);
    CHECK(session.output_selection[2]->elevation == 0);
    CHECK(session.output_labels[2] == "0:right.txt");

    qmap::clear_output_elevation(session, 2);

    CHECK_FALSE(session.output_selection[2].has_value());
    CHECK(session.output_labels[2] == "##2");
}

TEST_CASE("GUI session header selection updates export path and export plan", "[gui]")
{
    qmap::GuiSession session;
    session.right = loaded_map("right.txt", "C:/maps/right.txt");

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
    session.left = loaded_map("left.map", "C:/maps/left.map");
    session.left.map_type = qmap::MapFileKind::binary;
    session.right = loaded_map("right.txt", "C:/maps/right.txt");
    session.right.map_type = qmap::MapFileKind::text;

    const auto action = qmap::prepare_export(session);

    CHECK(action == qmap::GuiExportAction::none);
    CHECK(session.open_error_popup);
    CHECK(session.current_error.find("can't mix .MAP and .TXT") != std::string::npos);
}
