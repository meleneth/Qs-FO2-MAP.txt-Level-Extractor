#include <catch2/catch_test_macros.hpp>

#include "text_map_records.h"

#include <string_view>

TEST_CASE("parse_text_objects extracts object ranges and numeric fields", "[txt][records]")
{
    constexpr std::string_view objects =
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n"
        "[[OBJECTS BEGIN]]\r\n"
        "[OBJECT BEGIN]\r\n"
        "obj_elev: 2\r\n"
        "obj_sid: 50331649\r\n"
        "[OBJECT END]\r\n"
        "[[OBJECTS END]]\r\n";

    const auto parsed = qmap::parse_text_objects(objects);

    REQUIRE(parsed);
    REQUIRE(parsed.value().size() == 1);
    CHECK(parsed.value()[0].elevation == 2);
    CHECK(parsed.value()[0].script_id == 50331649u);
    CHECK(objects.substr(parsed.value()[0].raw.offset, parsed.value()[0].raw.size).starts_with("[OBJECT BEGIN]"));
}

TEST_CASE("parse_text_objects fails on unterminated object blocks", "[txt][records]")
{
    constexpr std::string_view objects =
        "[OBJECT BEGIN]\r\n"
        "obj_elev: 0\r\n";

    const auto parsed = qmap::parse_text_objects(objects);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "object record missing [OBJECT END]");
    CHECK(parsed.error().offset == 0);
}

TEST_CASE("parse_text_scripts extracts script records and numeric fields", "[txt][records]")
{
    constexpr std::string_view scripts =
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n"
        "SCRS:\r\n"
        "scr_num: 1\r\n"
        "scr_id: 16777216\r\n"
        "scr_next: 4294967295\r\n"
        "scr_flags: 0\r\n"
        "scr_script_idx: 221\r\n"
        "scr_oid: 1919251315\r\n"
        "scr_num_local_vars: 3\r\n\r\n"
        "scr_udata.sp.built_tile: 536870912\r\n\r\n"
        "scr_udata.sp.radius: 5\r\n"
        "scr_num: 1\r\n"
        "scr_id: 50331649\r\n"
        "scr_oid: 215\r\n"
        "scr_num_local_vars: 0\r\n";

    const auto parsed = qmap::parse_text_scripts(scripts);

    REQUIRE(parsed);
    REQUIRE(parsed.value().size() == 2);
    CHECK(parsed.value()[0].script_id == 16777216u);
    CHECK(parsed.value()[0].script_type == qmap::ScriptType::spatial);
    CHECK(parsed.value()[0].object_id == 1919251315u);
    CHECK(parsed.value()[0].local_var_count == 3);
    CHECK(parsed.value()[0].spatial_tile == 536870912);
    CHECK(parsed.value()[0].spatial_radius == 5);

    CHECK(parsed.value()[1].script_id == 50331649u);
    CHECK(parsed.value()[1].script_type == qmap::ScriptType::object);
    CHECK(parsed.value()[1].object_id == 215u);
    CHECK_FALSE(parsed.value()[1].spatial_tile.has_value());
}

TEST_CASE("parse_text_scripts rejects invalid script ids", "[txt][records]")
{
    constexpr std::string_view scripts =
        "scr_id: nope\r\n"
        "scr_num_local_vars: 0\r\n";

    const auto parsed = qmap::parse_text_scripts(scripts);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "script record has invalid scr_id");
}

TEST_CASE("parse_text_scripts rejects unsupported script types", "[txt][records]")
{
    constexpr std::string_view scripts =
        "scr_id: 83886081\r\n"
        "scr_num_local_vars: 0\r\n";

    const auto parsed = qmap::parse_text_scripts(scripts);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "script record has unsupported script type");
}
