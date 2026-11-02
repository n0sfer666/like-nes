#pragma once
#include <string>
namespace framework::input {
class RebindStore;
bool save_probe_profile(const RebindStore& store, const std::string& preset,
                        bool& prior_loaded, std::string& path);
}
