#include <catch2/catch_test_macros.hpp>

#include "cli_operations.h"

#include <filesystem>
#include <stdexcept>

TEST_CASE("single_elevation_plan selects one matching source elevation", "[cli]")
{
    const auto plan = qmap::cli::single_elevation_plan(2);

    CHECK(plan.header_side == qmap::MapSide::left);
    REQUIRE(plan.elevations[2]);
    CHECK(plan.elevations[2]->side == qmap::MapSide::left);
    CHECK(plan.elevations[2]->elevation == 2);
    CHECK_FALSE(plan.elevations[0]);
    CHECK_FALSE(plan.elevations[1]);
}

TEST_CASE("single_elevation_plan rejects invalid elevations", "[cli]")
{
    CHECK_THROWS_AS(qmap::cli::single_elevation_plan(-1), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::single_elevation_plan(3), std::runtime_error);
}

TEST_CASE("split_output_path creates stable elevation filenames", "[cli]")
{
    const auto path = qmap::cli::split_output_path("out", "vault.map.txt", 1);

    CHECK(path == std::filesystem::path("out") / "vault.map_elev1.txt");
}

TEST_CASE("apply_selection maps destination elevations to either input side", "[cli]")
{
    qmap::TextMapExportPlan plan;

    qmap::cli::apply_selection(plan, "0=L:2");
    qmap::cli::apply_selection(plan, "2=r:1");

    REQUIRE(plan.elevations[0]);
    CHECK(plan.elevations[0]->side == qmap::MapSide::left);
    CHECK(plan.elevations[0]->elevation == 2);
    REQUIRE(plan.elevations[2]);
    CHECK(plan.elevations[2]->side == qmap::MapSide::right);
    CHECK(plan.elevations[2]->elevation == 1);
    CHECK_FALSE(plan.elevations[1]);
}

TEST_CASE("apply_selection rejects malformed selections", "[cli]")
{
    qmap::TextMapExportPlan plan;

    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0=L"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "0=X:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "3=L:1"), std::runtime_error);
    CHECK_THROWS_AS(qmap::cli::apply_selection(plan, "1=L:3"), std::runtime_error);
}

TEST_CASE("lowercase_extension normalizes input paths", "[cli]")
{
    CHECK(qmap::cli::lowercase_extension("CITY.MAP.TXT") == ".txt");
    CHECK(qmap::cli::lowercase_extension("VAULT.MAP") == ".map");
}
