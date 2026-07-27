#include "platform_args.hpp"

namespace platform {

// POSIX отдаёт argv ровно теми байтами, что передал вызывающий, — перекодировки нет и чинить
// нечего. Копия всё равно делается: тип должен быть один на обе платформы, иначе шов пришлось
// бы обходить условной компиляцией у каждого main.
Args::Args(int& argc, char**& argv) {
    adopt(argc, argv);
    index(argc, argv);
}

} // namespace platform
