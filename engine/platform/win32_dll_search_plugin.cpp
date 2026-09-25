extern "C" __declspec(dllimport) int dll_search_dep_value();

extern "C" __declspec(dllexport) int dll_search_probe() { return dll_search_dep_value(); }
