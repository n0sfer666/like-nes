#include "platform_process.hpp"

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>

namespace platform {
namespace {

pid_t fork_exec(const std::vector<std::string>& argv) {
    std::vector<char*> c;
    c.reserve(argv.size() + 1);
    for (const auto& s : argv) c.push_back(const_cast<char*>(s.c_str()));
    c.push_back(nullptr);
    // fork копирует и буферы stdio. Не слив их, ребёнок при первом же flush повторно выплюнет
    // всё, что родитель ещё не дописал, — и вывод теста двоится, когда stdout не терминал.
    std::fflush(nullptr);
    const pid_t pid = fork();
    if (pid != 0) return pid;
    if (std::freopen("/dev/null", "w", stdout)) { /* best-effort */ }
    if (std::freopen("/dev/null", "w", stderr)) { /* best-effort */ }
    execvp(c[0], c.data());
    _exit(127);
}

int reap(pid_t pid) {
    int status = 0;
    int w = 0;
    do { w = waitpid(pid, &status, 0); } while (w < 0 && errno == EINTR); // EINTR-safe: не зомби
    return w < 0 ? -1 : status;
}

} // namespace

uint32_t process_id() { return static_cast<uint32_t>(getpid()); }

bool run_tool(const std::vector<std::string>& argv) {
    if (argv.empty()) return false;
    const pid_t pid = fork_exec(argv);
    if (pid < 0) return false;
    const int status = reap(pid);
    return status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

Child::~Child() { kill_and_wait(); }

Child::Child(Child&& o) noexcept : raw_(o.raw_) { o.raw_ = 0; }

Child& Child::operator=(Child&& o) noexcept {
    if (this != &o) {
        kill_and_wait();
        raw_ = o.raw_;
        o.raw_ = 0;
    }
    return *this;
}

bool Child::spawn(const std::vector<std::string>& argv) {
    if (argv.empty() || raw_ != 0) return false;
    const pid_t pid = fork_exec(argv);
    if (pid < 0) return false;
    raw_ = static_cast<intptr_t>(pid);
    return true;
}

bool Child::wait(ExitStatus& out) {
    out = ExitStatus{};   // не оставлять исход прошлого ребёнка в переиспользованной структуре
    if (raw_ == 0) return false;
    const pid_t pid = static_cast<pid_t>(raw_);
    raw_ = 0;
    const int status = reap(pid);
    if (status < 0) return false;
    if (WIFEXITED(status)) {
        out.kind = ExitKind::Exited;
        out.code = WEXITSTATUS(status);
        return true;
    }
    if (WIFSIGNALED(status)) {
        const int sig = WTERMSIG(status);
        out.code = sig;
        // SIGKILL/SIGTERM сюда приходят от того, кто останавливает игру снаружи; всё прочее —
        // падение самого процесса. SIGABRT в этом списке потому, что под ASan интенциональный
        // SIGSEGV перехватывается санитайзером и доезжает как abort() — исход тот же, крэш.
        out.kind = (sig == SIGKILL || sig == SIGTERM) ? ExitKind::Killed : ExitKind::Crashed;
        return true;
    }
    return false;
}

bool Child::kill_and_wait() {
    if (raw_ == 0) return false;
    const pid_t pid = static_cast<pid_t>(raw_);
    raw_ = 0;
    if (::kill(pid, SIGKILL) != 0 && errno != ESRCH) { reap(pid); return false; }
    const int status = reap(pid);
    return status >= 0 && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL;
}

} // namespace platform
