#include <catch2/catch_test_macros.hpp>

#include "text_map_export.h"

#include <string>
#include <string_view>

namespace {

struct ParsedFixture {
    std::string text;
    qmap::ParsedTextMap map;
};

ParsedFixture parse_fixture(std::string text)
{
    auto parsed = qmap::parse_text_map(text);
    REQUIRE(parsed);
    return {std::move(text), parsed.value()};
}

qmap::ParsedTextSource source_from(const ParsedFixture& fixture)
{
    return {fixture.text, fixture.map};
}

} // namespace

TEST_CASE("export_text_map writes one selected elevation with chosen header", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        "square_elev: 1\n\n"
        "right-one\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.header_side = qmap::MapSide::right;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::left, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().starts_with("right-header\r\nsquare_elev: 0\r\n\r\nleft-zero\r\n"));
    CHECK(exported.value().find(">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n") != std::string::npos);
    CHECK(exported.value().find(">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n") != std::string::npos);
}

TEST_CASE("export_text_map can place selected elevations in new positions", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\r\n"
        "square_elev: 0\r\n\r\n"
        "left-zero\r\n"
        "square_elev: 2\r\n\r\n"
        "left-two\r\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n"
    );
    const auto right = parse_fixture(
        "right-header\r\n"
        "square_elev: 1\r\n\r\n"
        "right-one\r\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\r\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\r\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::right, 1};
    plan.elevations[2] = qmap::ElevationSource{qmap::MapSide::left, 0};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE(exported);
    CHECK(exported.value().find("square_elev: 0\r\n\r\nright-one\r\n") != std::string::npos);
    CHECK(exported.value().find("square_elev: 1\r\n\r\n") == std::string::npos);
    CHECK(exported.value().find("square_elev: 2\r\n\r\nleft-zero\r\n") != std::string::npos);
}

TEST_CASE("export_text_map rejects absent source elevations", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        "square_elev: 0\n\n"
        "left-zero\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    const auto right = parse_fixture(
        "right-header\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::right, 1};

    const auto exported = qmap::export_text_map(source_from(left), source_from(right), plan);

    REQUIRE_FALSE(exported);
    CHECK(exported.error().message == "selected source elevation is absent");
}

TEST_CASE("export_text_map rejects invalid source elevation indexes", "[txt][export]")
{
    const auto left = parse_fixture(
        "left-header\n"
        ">>>>>>>>>>: SCRIPTS <<<<<<<<<<\n"
        ">>>>>>>>>>: OBJECTS <<<<<<<<<<\n"
    );
    qmap::TextMapExportPlan plan;
    plan.elevations[0] = qmap::ElevationSource{qmap::MapSide::left, 9};

    const auto exported = qmap::export_text_map(source_from(left), source_from(left), plan);

    REQUIRE_FALSE(exported);
    CHECK(exported.error().message == "invalid source elevation");
}
