#pragma once
#include <map>
#include <string>
#include <vector>

#include "tiled_import.hpp"

namespace tiled_fixture {

inline const char* const TMJ = R"({"compressionlevel":-1,"type":"map","version":"1.10","tiledversion":"1.10.2",
"orientation":"orthogonal","renderorder":"right-down","infinite":false,"backgroundcolor":"#80112233",
"width":4,"height":3,"tilewidth":16,"tileheight":16,"nextlayerid":9,"nextobjectid":4,
"tilesets":[{"firstgid":1,"source":"../art/tiles.tsj"},
 {"firstgid":9,"name":"deco","tilewidth":16,"tileheight":16,"tilecount":2,"columns":2,"margin":0,"spacing":0,
  "image":"deco.png","imagewidth":32,"imageheight":16}],
"layers":[
 {"id":1,"name":"sky","type":"imagelayer","image":"../art/sky.png","repeatx":true,"offsetx":4,"opacity":1,
  "visible":true,"x":0,"y":0,"properties":[{"name":"cover_y","type":"bool","value":false}]},
 {"id":2,"name":"world","type":"group","offsetx":16,"opacity":1,"visible":true,"x":0,"y":0,"layers":[
  {"id":3,"name":"solid","type":"tilelayer","width":4,"height":3,"offsety":-32,"opacity":1,"visible":true,
   "x":0,"y":0,"properties":[{"name":"collision","type":"bool","value":true},
                             {"name":"visible_too","type":"bool","value":true}],
   "data":[0,0,3,0, 0,2147483650,1,4, 1,1,1,1]}]},
 {"id":7,"name":"far","type":"group","opacity":0.5,"parallaxx":0.5,"offsety":8,"visible":true,"x":0,"y":0,
  "layers":[
  {"id":4,"name":"deco","type":"tilelayer","width":4,"height":3,"parallaxx":0.5,"opacity":0.5,"visible":true,
   "x":0,"y":0,"properties":[{"name":"repeat_y","type":"bool","value":true},
                             {"name":"cover_y","type":"bool","value":false}],
   "data":[6,0,0,0, 0,1610612745,0,0, 10,0,0,5]}]},
 {"id":8,"name":"off","type":"group","opacity":1,"visible":false,"x":0,"y":0,"layers":[
  {"id":5,"name":"hidden","type":"tilelayer","width":4,"height":3,"opacity":1,"visible":true,"x":0,"y":0,
   "data":[1,0,0,0, 0,0,0,0, 0,0,0,0]}]},
 {"id":6,"name":"things","type":"objectgroup","draworder":"topdown","offsetx":2,"opacity":1,"visible":true,
  "x":0,"y":0,"objects":[
  {"id":1,"name":"start","type":"spawn","point":true,"x":10,"y":20,"width":0,"height":0,"rotation":0,
   "visible":true,"properties":[{"name":"lives","type":"int","value":3},{"name":"speed","type":"float","value":1.5}]},
  {"id":2,"name":"door","type":"trigger","x":32,"y":16,"width":16,"height":32,"rotation":0,"visible":true,
   "properties":[{"name":"target","type":"string","value":"two"},{"name":"locked","type":"bool","value":true}]},
  {"id":3,"name":"pit","type":"","x":0,"y":0,"width":0,"height":0,"rotation":0,"visible":true,
   "polygon":[{"x":0,"y":0},{"x":16,"y":0},{"x":8,"y":8}]}]}]})";

inline const char* const TSJ = R"({"type":"tileset","version":"1.10","tiledversion":"1.10.2","name":"city",
"tilewidth":16,"tileheight":16,"tilecount":8,"columns":4,"margin":0,"spacing":0,
"image":"city.png","imagewidth":64,"imageheight":32,
"tiles":[{"id":0,"properties":[{"name":"flags","type":"string","value":"solid"}]},
 {"id":1,"properties":[{"name":"flags","type":"string","value":"solid slope_br"}]},
 {"id":2,"properties":[{"name":"flags","type":"string","value":"solid oneway"}]},
 {"id":3,"properties":[{"name":"flags","type":"string","value":"ladder"}]},
 {"id":4,"properties":[{"name":"note","type":"string","value":"lamp"}]},
 {"id":5,"animation":[{"tileid":6,"duration":100},{"tileid":7,"duration":260}]}]})";

inline const char* const PIPE_MAP = R"(
map       | one
tile_size | 16
origin    | 16 | -32
legend    | . | empty
legend    | X | solid
legend    | / | solid | slope_bl
legend    | - | solid | oneway
legend    | H | ladder
row       | ..-.
row       | ./XH
row       | XXXX
)";

constexpr uint64_t CITY = 0xC1, DECO = 0xD1, SKY = 0x51;

class MemSource final : public framework::tiled::Source {
public:
    std::map<std::string, std::string> files{{"../art/tiles.tsj", TSJ}};
    std::map<std::string, framework::tiled::Image> images{
        {"city.png", {CITY, 64, 32}}, {"deco.png", {DECO, 32, 16}}, {"../art/sky.png", {SKY, 128, 64}}};
    std::vector<std::string> asked;

    bool tileset(const std::string& from, const std::string& rel, std::string& file, std::vector<uint8_t>& bytes,
                 std::string& error) override {
        asked.push_back(from + " -> " + rel);
        const auto it = files.find(rel);
        if (it == files.end()) {
            error = "no file " + rel;
            return false;
        }
        file = "art/tiles.tsj";
        bytes.assign(it->second.begin(), it->second.end());
        return true;
    }
    bool image(const std::string& from, const std::string& rel, framework::tiled::Image& out,
               std::string& error) override {
        asked.push_back(from + " -> " + rel);
        const auto it = images.find(rel);
        if (it == images.end()) {
            error = "no image " + rel;
            return false;
        }
        out = it->second;
        return true;
    }
};

inline bool import(const std::string& tmj, MemSource& src, framework::tiled::Level& out, std::string& error) {
    const std::vector<uint8_t> bytes(tmj.begin(), tmj.end());
    return framework::tiled::import_tmj("one", "levels/one.tmj", std::as_bytes(std::span<const uint8_t>(bytes)),
                                        src, out, error);
}

} // namespace tiled_fixture
