#include "build_orchestrator.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace ide::build {

BuildResult run_build(const std::vector<std::string>& argv) {
    BuildResult r;
    if (argv.empty()) return r;

    int pipefd[2];
    if (pipe(pipefd) != 0) return r;
    // CLOEXEC на оба конца: в child dup2→stdout/stderr даёт свежие не-CLOEXEC fd (переживают exec),
    // а исходные pipe-fd + любые host-fd не текут в exec'нутый компилятор.
    fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) { close(pipefd[0]); close(pipefd[1]); return r; }

    if (pid == 0) {
        // child: stdout+stderr → pipe write end, exec компилятора
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }

    close(pipefd[1]);
    char buf[4096];
    for (;;) {
        ssize_t n = read(pipefd[0], buf, sizeof(buf));
        if (n > 0) { r.raw_output.append(buf, static_cast<size_t>(n)); continue; }
        if (n < 0 && errno == EINTR) continue;   // EINTR-safe: не терять вывод/диагностики
        break;                                    // EOF (0) или реальная ошибка
    }
    close(pipefd[0]);

    int st = 0;
    pid_t w;
    do { w = waitpid(pid, &st, 0); } while (w < 0 && errno == EINTR);   // EINTR-safe: не зомби
    r.exit_code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    r.success = (r.exit_code == 0);
    r.diagnostics = parse_diagnostics(r.raw_output);
    return r;
}

bool file_changed(const std::string& path, int64_t& last_token) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;   // отсутствие/удаление ≠ изменение
#if defined(__APPLE__)
    int64_t m = static_cast<int64_t>(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
    int64_t m = static_cast<int64_t>(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
    // Композит mtime+size: детект правки даже при грубой (1с) mtime-гранулярности ФС, если сменился
    // размер. (Одинаковый размер в ту же секунду — редкий промах; nsec-mtime на APFS/ext4 снимает.)
    // Микс в uint64 (обёртка определена) → храним как непрозрачный int64-токен.
    uint64_t mix = static_cast<uint64_t>(m) * 1099511628211ull + static_cast<uint64_t>(st.st_size);
    int64_t token = static_cast<int64_t>(mix);
    if (token != last_token) { last_token = token; return true; }
    return false;
}

} // namespace ide::build
