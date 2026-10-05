#include <cstdio>
#include <string>
#include <vector>

#include "tiled_fixture.hpp"

namespace {

int fails = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        ++fails;
    }
}

enum class In { Map, Tileset };

struct Edit {
    In in;
    const char* from;
    const char* to;
};

struct Case {
    const char* what;
    std::vector<Edit> edits;
    const char* message;
    uint32_t deco_width = 32;
};

const char* const OBJ_DOOR = R"("name":"door",)";
const char* const DECO_TS = R"("firstgid":9,"name":"deco",)";
const char* const DECO_LAYER = R"("name":"deco","type":"tilelayer",)";
const char* const ROOT = R"("compressionlevel":-1,)";

const Case CASES[] = {
    {"infinite map", {{In::Map, R"("infinite":false)", R"("infinite":true)"}}, "Infinite off"},
    {"base64 layer", {{In::Map, DECO_LAYER, R"("name":"deco","type":"tilelayer","encoding":"base64",)"}},
     "set Tile Layer Format to CSV"},
    {"compressed layer", {{In::Map, DECO_LAYER, R"("name":"deco","type":"tilelayer","compression":"zlib",)"}},
     "set Tile Layer Format to CSV"},
    {"animation through an animated tile",
     {{In::Tileset, R"({"id":4,"properties":[{"name":"note","type":"string","value":"lamp"}]})",
       R"({"id":4,"animation":[{"tileid":5,"duration":100}]})"}},
     "Tiled draws such a frame static"},
    {"slope flipped vertically", {{In::Map, "2147483650", "1073741826"}}, "only a horizontal flip mirrors a slope"},
    {"slope rotated", {{In::Map, "2147483650", "536870914"}}, "only a horizontal flip mirrors a slope"},
    {"class differs from type", {{In::Map, R"("type":"trigger",)", R"("type":"trigger","class":"spawn",)"}},
     "keep one"},
    {"ellipse", {{In::Map, OBJ_DOOR, R"("name":"door","ellipse":true,)"}}, "is an ellipse"},
    {"polyline", {{In::Map, OBJ_DOOR, R"("name":"door","polyline":[{"x":0,"y":0}],)"}}, "is a polyline"},
    {"text object", {{In::Map, OBJ_DOOR, R"("name":"door","text":{"text":"hi"},)"}}, "is a text object"},
    {"tile object", {{In::Map, OBJ_DOOR, R"("name":"door","gid":1,)"}}, "is a tile object"},
    {"template", {{In::Map, OBJ_DOOR, R"("name":"door","template":"door.tx",)"}}, "detach it"},
    {"rotated object", {{In::Map, R"("height":32,"rotation":0,)", R"("height":32,"rotation":90,)"}}, "is rotated"},
    {"color property", {{In::Map, R"("name":"target","type":"string","value":"two")",
                         R"("name":"target","type":"color","value":"#ff00ff00")"}},
     "has type color"},
    {"int property past 32 bits", {{In::Map, R"("type":"int","value":3)", R"("type":"int","value":4294967296)"}},
     "does not fit 32 bits"},
    {"polygon of two points", {{In::Map, R"(,{"x":8,"y":8}])", "]"}}, "allowed 3 to 16"},
    {"duplicate object id", {{In::Map, R"("id":2,"name":"door")", R"("id":1,"name":"door")"}}, "used twice"},
    {"tint color", {{In::Map, DECO_LAYER, R"("name":"deco","type":"tilelayer","tintcolor":"#ff0000",)"}},
     "uses a tint color"},
    {"opacity past 1", {{In::Map, R"("parallaxx":0.5,"opacity":0.5)", R"("parallaxx":0.5,"opacity":1.5)"}},
     "opacity outside 0..1"},
    {"parallax origin", {{In::Map, ROOT, R"("compressionlevel":-1,"parallaxoriginx":8,)"}}, "Parallax Origin"},
    {"isometric map", {{In::Map, R"("orientation":"orthogonal")", R"("orientation":"isometric")"}},
     "draws orthogonal maps"},
    {"bad background", {{In::Map, R"("#80112233")", R"("#8011223")"}}, "#RRGGBB or #AARRGGBB"},
    {"hex rotation bit 28", {{In::Map, "10,0,0,5]", "268435466,0,0,5]"}}, "hexagonal rotation bit 28"},
    {"gid past every tileset", {{In::Map, "10,0,0,5]", "11,0,0,5]"}}, "belongs to no tileset"},
    {"more than 8191 tiles",
     {{In::Map, R"("tilecount":2,"columns":2,)", R"("tilecount":8184,"columns":8184,)"},
      {In::Map, R"("imagewidth":32,)", R"("imagewidth":130944,)"}},
     "more than 8191 tiles", 130944},
    {"collision tile without flags", {{In::Map, "0,2147483650,1,4,", "0,2147483650,1,5,"}},
     "has no 'flags' property"},
    {"unknown flag word", {{In::Tileset, R"("value":"ladder")", R"("value":"ledge")"}}, "unknown tile flag 'ledge'"},
    {"image collection", {{In::Map, R"("image":"deco.png",)", ""}}, "is a collection of images"},
    {"non-square map tiles", {{In::Map, R"("tilewidth":16,"tileheight":16,"nextlayerid")",
                               R"("tilewidth":16,"tileheight":8,"nextlayerid")"}},
     "not square"},
    {"non-square tileset tiles", {{In::Tileset, R"("tileheight":16)", R"("tileheight":8)"}}, "non-square tiles"},
    {"tileset tile size differs", {{In::Map, R"("name":"deco","tilewidth":16,"tileheight":16)",
                                    R"("name":"deco","tilewidth":8,"tileheight":8)"}},
     "one tile size per map"},
    {"tile offset", {{In::Map, DECO_TS, R"("firstgid":9,"name":"deco","tileoffset":{"x":0,"y":4},)"}},
     "Tile Offset to 0,0"},
    {"tileset transparent color", {{In::Map, DECO_TS, R"("firstgid":9,"name":"deco","transparentcolor":"#ff00ff",)"}},
     "uses a transparent color"},
    {"image layer transparent color",
     {{In::Map, R"("repeatx":true,)", R"("repeatx":true,"transparentcolor":"#ff00ff",)"}},
     "uses a transparent color"},
    {"cover_y not a bool", {{In::Map, R"("name":"cover_y","type":"bool","value":false}]},)",
                             R"("name":"cover_y","type":"string","value":"no"}]},)"}},
     "property 'cover_y' must be of type bool"},
    {"xml tileset", {{In::Map, "../art/tiles.tsj", "../art/tiles.tsx"}}, "export it as JSON (.tsj)"},
    {"image size differs from the file", {{In::Map, R"("imagewidth":32,)", R"("imagewidth":48,)"}},
     "reload the image in Tiled"},
    {"overlapping firstgid", {{In::Map, R"("firstgid":9,)", R"("firstgid":5,)"}}, "overlaps the previous tileset"},
    {"frame outside the tileset", {{In::Tileset, R"("tileid":7,)", R"("tileid":8,)"}}, "is outside tileset"},
    {"frame duration past 10 minutes", {{In::Tileset, R"("duration":260)", R"("duration":600001)"}},
     "use 0 to 600000"},
    {"no collision layer", {{In::Map, R"({"name":"collision","type":"bool","value":true},)", ""}},
     "has no collision layer"},
    {"second collision layer",
     {{In::Map, R"("name":"hidden","type":"tilelayer",)",
       R"("name":"hidden","type":"tilelayer","properties":[{"name":"collision","type":"bool","value":true}],)"}},
     "second collision layer"},
    {"flipped ladder", {{In::Map, "0,2147483650,1,4,", "0,2147483650,1,2147483652,"}}, "is flipped or rotated"},
    {"group offset past fix32", {{In::Map, R"("offsetx":16,)", R"("offsetx":32767,)"},
                                 {In::Map, R"("offsety":-32,)", R"("offsetx":2,"offsety":-32,)"}},
     "adds up to an offset or parallax past"},
    {"duplicate tile id", {{In::Tileset, R"({"id":4,)", R"({"id":3,)"}}, "tile id 3 appears twice in tileset 'city'"},
    {"duplicate property", {{In::Map, R"("name":"locked")", R"("name":"target")"}}, "has property 'target' twice"},
    {"collision layer with parallax", {{In::Map, R"("name":"world","type":"group",)",
                                        R"("name":"world","type":"group","parallaxy":0.5,)"}},
     "has parallax; keep parallax 1"},
    {"object layer with parallax", {{In::Map, R"("draworder":"topdown",)", R"("draworder":"topdown","parallaxx":2,)"}},
     "objects live in the world"},
    {"layer size differs", {{In::Map, R"("name":"hidden","type":"tilelayer","width":4)",
                             R"("name":"hidden","type":"tilelayer","width":3)"}},
     "does not match the map size"},
    {"missing tileset file", {{In::Map, "../art/tiles.tsj", "../art/none.tsj"}}, "no file ../art/none.tsj"},
    {"missing tileset image", {{In::Map, R"("image":"deco.png",)", R"("image":"gone.png",)"}}, "no image gone.png"},
    {"broken JSON", {{In::Map, ROOT, R"("compressionlevel":-1,,)"}}, "levels/one.tmj:1:"},
    {"unknown layer type", {{In::Map, R"("type":"imagelayer")", R"("type":"hexlayer")"}}, "unknown layer type"},
};

bool apply(const Edit& e, std::string& tmj, std::string& tsj) {
    std::string& text = e.in == In::Map ? tmj : tsj;
    const size_t at = text.find(e.from);
    if (at == std::string::npos || text.find(e.from, at + 1) != std::string::npos) return false;
    text.replace(at, std::string(e.from).size(), e.to);
    return true;
}

} // namespace

int main() {
    for (const Case& c : CASES) {
        std::string tmj = tiled_fixture::TMJ, tsj = tiled_fixture::TSJ;
        bool unique = true;
        for (const Edit& e : c.edits) unique = apply(e, tmj, tsj) && unique;
        check(unique, std::string(c.what) + ": the edit matches the fixture exactly once");
        tiled_fixture::MemSource src;
        src.files["../art/tiles.tsj"] = tsj;
        src.images["deco.png"].width = c.deco_width;
        framework::tiled::Level level;
        std::string error;
        const bool ok = tiled_fixture::import(tmj, src, level, error);
        const bool reason = error.find(c.message) != std::string::npos;
        check(!ok && reason, std::string(c.what) + " is refused for its own reason");
        if (ok || !reason) std::printf("    got: %s\n", ok ? "(imported)" : error.c_str());
    }
    std::printf("framework-tiled-refusal: %s - %zu cases\n", fails == 0 ? "PASS" : "FAIL",
                sizeof(CASES) / sizeof(CASES[0]));
    return fails == 0 ? 0 : 1;
}
