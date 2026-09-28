#include "platform_process.hpp"
#include "platform_capture.hpp"
#include "platform_spawn_posix.hpp"

#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

namespace platform {
namespace {

// Держатель действий и атрибутов posix_spawn: обе структуры обязаны быть разрушены на любом
// выходе из spawn_child.
struct SpawnSetup {
    posix_spawn_file_actions_t fa;
    posix_spawnattr_t attr;
    bool ok = false;

    SpawnSetup() {
        if (posix_spawn_file_actions_init(&fa) != 0) return;
        if (posix_spawnattr_init(&attr) != 0) {
            posix_spawn_file_actions_destroy(&fa);
            return;
        }
        ok = true;
    }
    ~SpawnSetup() {
        if (!ok) return;
        posix_spawnattr_destroy(&attr);
        posix_spawn_file_actions_destroy(&fa);
    }
    SpawnSetup(const SpawnSetup&) = delete;
    SpawnSetup& operator=(const SpawnSetup&) = delete;
};

// out_fd >= 0 — stdout и stderr ребёнка уезжают туда; иначе оба в /dev/null.
//
// posix_spawn, а не fork + execvp: между ними исполнялся бы наш код в копии многопоточного
// процесса (freopen там берёт блокировку stdio, которую чужой поток родителя мог унести с собой),
// и ребёнок наследовал бы каждый дескриптор без CLOEXEC — привязанный сокет держал порт занятым,
// пока жив ребёнок. Ребёнку достаются только 0, 1 и 2, как на Windows.
pid_t spawn_child(const std::vector<std::string>& argv, int out_fd = -1) {
    std::vector<char*> c;
    c.reserve(argv.size() + 1);
    for (const auto& s : argv) c.push_back(const_cast<char*>(s.c_str()));
    c.push_back(nullptr);

    SpawnSetup setup;
    if (!setup.ok) return -1;
    posix_spawn_file_actions_t& fa = setup.fa;
    const bool routed =
        out_fd >= 0
            ? posix_spawn_file_actions_adddup2(&fa, out_fd, STDOUT_FILENO) == 0 &&
                  posix_spawn_file_actions_adddup2(&fa, out_fd, STDERR_FILENO) == 0
            : posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null", O_WRONLY, 0) == 0 &&
                  posix_spawn_file_actions_adddup2(&fa, STDOUT_FILENO, STDERR_FILENO) == 0;
    if (!routed || !posix::inherit_only_std(setup.attr, fa)) return -1;

    pid_t pid = -1;
    if (posix_spawnp(&pid, c[0], &fa, &setup.attr, c.data(), posix::current_environment()) != 0)
        return -1;
    return pid;
}

int reap(pid_t pid) {
    int status = 0;
    int w = 0;
    do { w = waitpid(pid, &status, 0); } while (w < 0 && errno == EINTR); // EINTR-safe: не зомби
    return w < 0 ? -1 : status;
}

bool classify(int status, ExitStatus& out) {
    if (status < 0) return false;
    if (WIFEXITED(status)) {
        out.kind = ExitKind::Exited;
        out.code = WEXITSTATUS(status);
        return true;
    }
    if (WIFSIGNALED(status)) {
        const int sig = WTERMSIG(status);
        out.code = sig;
        // SIGKILL/SIGTERM сюда приходят от того, кто останавливает процесс снаружи; всё прочее —
        // падение самого процесса, включая SIGABRT. NB: санитайзерный прогон это НЕ покрывает —
        // ASan с перехватом SEGV завершает процесс штатным exit(1) и сюда не попадает вовсе,
        // поэтому крэш-гейты гоняются с handle_segv=0 (см. шаг ASan в ci.yml).
        out.kind = (sig == SIGKILL || sig == SIGTERM) ? ExitKind::Killed : ExitKind::Crashed;
        return true;
    }
    return false;
}

} // namespace

uint32_t process_id() { return static_cast<uint32_t>(getpid()); }

bool run_tool(const std::vector<std::string>& argv) {
    if (argv.empty()) return false;
    const pid_t pid = spawn_child(argv);
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
    const pid_t pid = spawn_child(argv);
    if (pid < 0) return false;
    raw_ = static_cast<intptr_t>(pid);
    return true;
}

bool Child::wait(ExitStatus& out) {
    out = ExitStatus{};   // не оставлять исход прошлого ребёнка в переиспользованной структуре
    if (raw_ == 0) return false;
    const pid_t pid = static_cast<pid_t>(raw_);
    raw_ = 0;
    return classify(reap(pid), out);
}

bool run_capture(const std::vector<std::string>& argv, std::string& output, ExitStatus& status,
                 size_t max_output) {
    output.clear();
    status = ExitStatus{};
    if (argv.empty()) return false;

    int fds[2];
    if (pipe(fds) != 0) return false;
    // Оба конца поднимаются выше стандартных дескрипторов: у вызывающего с закрытым stdout pipe()
    // вернул бы fd 1, и dup2(1, 1) в ребёнке оказался бы no-op, который НЕ снимает CLOEXEC —
    // вывод терялся бы после exec, а run_capture тихо отдавал пустую строку и Exited(0).
    for (int i = 0; i < 2; ++i) {
        if (fds[i] > STDERR_FILENO) continue;
        const int hi = fcntl(fds[i], F_DUPFD, STDERR_FILENO + 1);
        if (hi < 0) { ::close(fds[0]); ::close(fds[1]); return false; }
        ::close(fds[i]);
        fds[i] = hi;
    }
    // CLOEXEC на оба конца: ребёнку нужны только dup2-копии (они CLOEXEC не наследуют), а сам
    // канал в exec'нутую программу течь не должен — иначе она держит конец записи и мы не увидим
    // EOF, пока жив хоть один её потомок. Провал именно здесь — вечное зависание на read ниже,
    // единственный отказ в этой функции, который нельзя проглотить.
    if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(fds[1], F_SETFD, FD_CLOEXEC) != 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return false;
    }

    const pid_t pid = spawn_child(argv, fds[1]);
    ::close(fds[1]);
    if (pid < 0) { ::close(fds[0]); return false; }

    char buf[4096];
    bool truncated = false;
    for (;;) {
        const ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n > 0) {
            if (append_capped(output, buf, static_cast<size_t>(n), max_output)) continue;
            truncated = true;
            break;
        }
        if (n < 0 && errno == EINTR) continue;   // EINTR-safe: не терять диагностики
        break;                                    // EOF (0) либо реальная ошибка
    }
    ::close(fds[0]);
    if (truncated) ::kill(pid, SIGKILL);
    const bool reaped = classify(reap(pid), status);
    if (reaped && truncated) status.kind = ExitKind::Truncated;
    return reaped;
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
