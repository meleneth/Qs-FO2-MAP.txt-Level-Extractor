#include <catch2/catch_test_macros.hpp>

#include "text_map_parser.h"

#include <string_view>

namespace {

constexpr std::string_view crlf_map =
    "header: left\r\n"
    "square_elev: 0\r\n\r\n"
    "tiles-0\r\n"
    "square_elev: 1\r\n\r\n"
    "tiles-1\r\n"
    ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n"
    "SCRS:\r\n"
    ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n"
    "[[OBJECTS BEGIN]]\r\n";

constexpr std::string_view lf_map =
    "header: left\n"
    "square_elev: 0\n\n"
    "tiles-0\n"
    "square_elev: 2\n\n"
    "tiles-2\n"
    ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
    "SCRS:\n"
    ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    "[[OBJECTS BEGIN]]\n";

} // namespace

TEST_CASE("parse_text_map locates CRLF elevation ranges", "[txt][bounded]")
{
    const auto parsed = qmap::parse_text_map(crlf_map);

    REQUIRE(parsed);
    REQUIRE(parsed.value().header_view(crlf_map));
    CHECK(*parsed.value().header_view(crlf_map) == "header: left\r\n");

    REQUIRE(parsed.value().elevation_view(crlf_map, 0));
    CHECK(*parsed.value().elevation_view(crlf_map, 0) == "tiles-0\r\n");

    REQUIRE(parsed.value().elevation_view(crlf_map, 1));
    CHECK(*parsed.value().elevation_view(crlf_map, 1) == "tiles-1\r\n");

    CHECK_FALSE(parsed.value().elevation_view(crlf_map, 2).has_value());
}

TEST_CASE("parse_text_map locates LF elevation ranges with missing middle elevation", "[txt][bounded]")
{
    const auto parsed = qmap::parse_text_map(lf_map);

    REQUIRE(parsed);
    REQUIRE(parsed.value().elevation_view(lf_map, 0));
    CHECK(*parsed.value().elevation_view(lf_map, 0) == "tiles-0\n");

    CHECK_FALSE(parsed.value().elevation_view(lf_map, 1).has_value());

    REQUIRE(parsed.value().elevation_view(lf_map, 2));
    CHECK(*parsed.value().elevation_view(lf_map, 2) == "tiles-2\n");
}

TEST_CASE("parse_text_map treats all text before first elevation as header", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header-only\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE(parsed);
    REQUIRE(parsed.value().header_view(text));
    CHECK(*parsed.value().header_view(text) == "header-only\n");
    CHECK_FALSE(parsed.value().elevation_view(text, 0).has_value());
}

TEST_CASE("parse_text_map ignores elevation marker text that is not at line start", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header mentions square_elev: 0\n\n"
        "still header\n"
        "square_elev: 0\n\n"
        "tiles-0\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE(parsed);
    REQUIRE(parsed.value().header_view(text));
    CHECK(*parsed.value().header_view(text) == "header mentions square_elev: 0\n\nstill header\n");
    REQUIRE(parsed.value().elevation_view(text, 0));
    CHECK(*parsed.value().elevation_view(text, 0) == "tiles-0\n");
}

TEST_CASE("parse_text_map returns section ranges", "[txt][bounded]")
{
    const auto parsed = qmap::parse_text_map(crlf_map);

    REQUIRE(parsed);
    REQUIRE(parsed.value().scripts_view(crlf_map));
    CHECK(parsed.value().scripts_view(crlf_map)->starts_with(">>>>>>>>>>: SCRIPTS"));
    CHECK(parsed.value().scripts_view(crlf_map)->ends_with("SCRS:\r\n"));

    REQUIRE(parsed.value().objects_view(crlf_map));
    CHECK(parsed.value().objects_view(crlf_map)->starts_with(">>>>>>>>>>: OBJECTS"));
    CHECK(parsed.value().objects_view(crlf_map)->ends_with("[[OBJECTS BEGIN]]\r\n"));
}

TEST_CASE("parse_text_map ignores section marker text that is not at line start", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header mentions >>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "header mentions >>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "square_elev: 0\n\n"
        "tiles-0\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE(parsed);
    REQUIRE(parsed.value().scripts_view(text));
    CHECK(parsed.value().scripts_view(text)->starts_with(">>>>>>>>>>: SCRIPTS"));
    REQUIRE(parsed.value().objects_view(text));
    CHECK(parsed.value().objects_view(text)->starts_with(">>>>>>>>>>: OBJECTS"));
}

TEST_CASE("parse_text_map fails when required sections are missing", "[txt][bounded]")
{
    const auto no_scripts = qmap::parse_text_map("header\n>>>>>>>>>>: OBJECTS <<<<<<<<<<\n");
    REQUIRE_FALSE(no_scripts);
    CHECK(no_scripts.error().message == "missing SCRIPTS section");

    const auto no_objects = qmap::parse_text_map("header\n>>>>>>>>>>: SCRIPTS <<<<<<<<<<\n");
    REQUIRE_FALSE(no_objects);
    CHECK(no_objects.error().message == "missing OBJECTS section");
}

TEST_CASE("parse_text_map fails when objects appear before scripts", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "OBJECTS section appears before SCRIPTS section");
}

TEST_CASE("parse_text_map fails when elevation markers are out of order", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header\n"
        "square_elev: 2\n\n"
        "tiles-2\n"
        "square_elev: 1\n\n"
        "tiles-1\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "elevation markers are out of order");
}

TEST_CASE("parse_text_map fails when an elevation marker appears twice", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header\n"
        "square_elev: 0\n\n"
        "tiles-0-a\n"
        "square_elev: 0\n\n"
        "tiles-0-b\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "duplicate elevation marker");
}

TEST_CASE("parse_text_map fails when duplicate elevation markers use mixed line endings", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header\r\n"
        "square_elev: 1\r\n\r\n"
        "tiles-1-a\r\n"
        "square_elev: 1\n\n"
        "tiles-1-b\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "duplicate elevation marker");
}

TEST_CASE("parse_text_map fails when the scripts section appears twice", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header\n"
        "square_elev: 0\n\n"
        "tiles-0\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "duplicate SCRIPTS section");
}

TEST_CASE("parse_text_map fails when the objects section appears twice", "[txt][bounded]")
{
    constexpr std::string_view text =
        "header\n"
        "square_elev: 0\n\n"
        "tiles-0\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        "SCRS:\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
        "[[OBJECTS BEGIN]]\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n";

    const auto parsed = qmap::parse_text_map(text);

    REQUIRE_FALSE(parsed);
    CHECK(parsed.error().message == "duplicate OBJECTS section");
}
