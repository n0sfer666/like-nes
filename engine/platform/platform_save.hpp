#pragma once
#include <string>

namespace platform {
// Shell-owned process state, game thread only, configured before game init. No filesystem IO.
// Empty/NUL/relative path explicitly disables saving; it must not become an OS/CWD fallback.
// Returns whether the absolute path was accepted (not whether its directory is writable).
bool set_save_dir(std::string directory);
// nullptr: shell did not choose a directory; pointer to empty: shell explicitly refused it.
const std::string* save_dir_override();
}
