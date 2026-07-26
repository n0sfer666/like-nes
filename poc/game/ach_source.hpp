#pragma once
#include <memory>
#include <string>

namespace ach {
class Registry;
}

namespace game {

// Шов чтения каталога достижений из бандла #5. Реализация bundle-версии тянет asset_core
// (mmap, POSIX) — на Windows подставляется stub, каталог остаётся пустым до рантайм-регистрации.
class AchSource {
public:
    AchSource();
    ~AchSource();
    AchSource(const AchSource&) = delete;
    AchSource& operator=(const AchSource&) = delete;

    bool open(ach::Registry& reg, const std::string& bundle_path);
    const char* reason() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace game
