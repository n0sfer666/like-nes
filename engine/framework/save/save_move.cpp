#include "save_io.hpp"

#include <cstdio>

#include "platform_fs.hpp"

namespace save::detail {

MoveResult move_new_paths(const std::string& source, const std::string& target) {
    if (source.empty() || target.empty() || source == target) return MoveResult::Error;
    if (!platform::copy_file_new(source, target))
        return platform::file_exists(target) || platform::is_dir(target) ? MoveResult::Exists : MoveResult::Error;
    // Не удаляем оригинал до долговечного publish новой копии. copy_file_new не
    // перезаписывает чужой карантин; неудачный sync убирает лишь созданную нами копию.
    std::FILE* copy = platform::open_file(target, "r+b");
    const bool synced = copy && platform::sync_file(copy);
    const bool closed = copy ? std::fclose(copy) == 0 : false;
    if (!synced || !closed || !platform::sync_dir_of(target)) {
        platform::remove_file(target);
        return MoveResult::Error;
    }
    if (!platform::remove_file(source)) return MoveResult::Error;
    // Backup уже зафиксирован. Неподтверждённый unlink после crash может вернуть
    // оригинал, но не потерять обе копии; Error после успешного unlink нарушил бы контракт.
    (void)platform::sync_dir_of(source);
    return MoveResult::Moved;
}

} // namespace save::detail
