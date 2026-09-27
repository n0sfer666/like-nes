#include "platform_process.hpp"

#include <unistd.h>

// Android: приложение не запускает чужие процессы — posix_spawn в bionic появляется только с API 28,
// закрытие лишних дескрипторов ребёнка — с API 34, а песочница всё равно не пускает exec сторонних
// инструментов. Потребители шва — пекарь, build-loop, Play-spawn — живут на десктопе, поэтому здесь
// явный отказ: каждый вызов отвечает «не запустился», а не делает вид, что ребёнок был.
namespace platform {

uint32_t process_id() { return static_cast<uint32_t>(getpid()); }

bool run_tool(const std::vector<std::string>&) { return false; }

bool run_capture(const std::vector<std::string>&, std::string& output, ExitStatus& status, size_t) {
    output.clear();
    status = ExitStatus{};
    return false;
}

Child::~Child() = default;

Child::Child(Child&& o) noexcept : raw_(o.raw_) { o.raw_ = 0; }

Child& Child::operator=(Child&& o) noexcept {
    raw_ = o.raw_;
    o.raw_ = 0;
    return *this;
}

bool Child::spawn(const std::vector<std::string>&) { return false; }

bool Child::kill_and_wait() { return false; }

bool Child::wait(ExitStatus& out) {
    out = ExitStatus{};
    return false;
}

} // namespace platform
