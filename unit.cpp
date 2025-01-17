#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include "text.hpp"
#include "pmtiles.hpp"

TEST_CASE("UTF-8 enforcement", "[utf8]") {
	REQUIRE(check_utf8("") == std::string(""));
	REQUIRE(check_utf8("hello world") == std::string(""));
	REQUIRE(check_utf8("Καλημέρα κόσμε") == std::string(""));
	REQUIRE(check_utf8("こんにちは 世界") == std::string(""));
	REQUIRE(check_utf8("👋🌏") == std::string(""));
	REQUIRE(check_utf8("Hola m\xF3n") == std::string("\"Hola m\xF3n\" is not valid UTF-8 (0xF3 0x6E)"));
}

TEST_CASE("UTF-8 truncation", "[trunc]") {
	REQUIRE(truncate16("0123456789abcdefghi", 16) == std::string("0123456789abcdef"));
	REQUIRE(truncate16("0123456789éîôüéîôüç", 16) == std::string("0123456789éîôüéî"));
	REQUIRE(truncate16("0123456789😀😬😁😂😃😄😅😆", 16) == std::string("0123456789😀😬😁"));
	REQUIRE(truncate16("0123456789😀😬😁😂😃😄😅😆", 17) == std::string("0123456789😀😬😁"));
	REQUIRE(truncate16("0123456789あいうえおかきくけこさ", 16) == std::string("0123456789あいうえおか"));
}

TEST_CASE("TileID to Z,X,Y", "") {
	auto result = tileid_to_zxy(0);
	REQUIRE(result.z == 0);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 0);
	result = tileid_to_zxy(1);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 0);
	result = tileid_to_zxy(2);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 1);
	result = tileid_to_zxy(3);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 1);
	REQUIRE(result.y == 1);
	result = tileid_to_zxy(4);
	REQUIRE(result.z == 1);
	REQUIRE(result.x == 1);
	REQUIRE(result.y == 0);
	result = tileid_to_zxy(5);
	REQUIRE(result.z == 2);
	REQUIRE(result.x == 0);
	REQUIRE(result.y == 0);
}

TEST_CASE("Z,X,Y to TileID", "") {
	REQUIRE(zxy_to_tileid(0,0,0) == 0);
	REQUIRE(zxy_to_tileid(1,0,0) == 1);
	REQUIRE(zxy_to_tileid(1,0,1) == 2);
	REQUIRE(zxy_to_tileid(1,1,1) == 3);
	REQUIRE(zxy_to_tileid(1,1,0) == 4);
	REQUIRE(zxy_to_tileid(2,0,0) == 5);
}

TEST_CASE("serialize pmtiles directory", "") {
	std::vector<pmtilesv3_entry> entries;
	entries.push_back(pmtilesv3_entry(0,0,0,0));
	entries.push_back(pmtilesv3_entry(1,1,1,1));
	entries.push_back(pmtilesv3_entry(2,2,2,2));
	auto serialized = serialize_entries(entries);
	auto result = deserialize_entries(serialized);
	REQUIRE(result.size() == 3);
	REQUIRE(result[0].tile_id == 0);
	REQUIRE(result[0].offset == 0);
	REQUIRE(result[0].length == 0);
	REQUIRE(result[0].run_length == 0);
	REQUIRE(result[1].tile_id == 1);
	REQUIRE(result[1].offset == 1);
	REQUIRE(result[1].length == 1);
	REQUIRE(result[1].run_length == 1);
	REQUIRE(result[2].tile_id == 2);
	REQUIRE(result[2].offset == 2);
	REQUIRE(result[2].length == 2);
	REQUIRE(result[2].run_length == 2);
}
