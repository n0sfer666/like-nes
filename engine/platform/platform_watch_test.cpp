#include "platform_watch.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "platform_env.hpp"
#include "platform_fs.hpp"
#include "platform_process.hpp"

// Шов platform_watch (спека #13, решение 4) на трёх ОС. Проверяется КАЖДЫЙ бэкенд: нативный и
// принудительный поллинг (LIKE_NES_WATCH=poll) — иначе фолбэк, который включается только на
// сетевой ФС, не исполнялся бы ни в одном прогоне и сгнил бы молча.
//
// Сравнение идёт по ИМЕНИ файла, а не по полному пути: формы путей у бэкендов разные и это
// нормально (FSEvents канонизирует /tmp → /private/tmp, Windows отдаёт обратные слэши).
namespace {

int failures = 0;
void check(bool c, const char* what) {
    if (!c) {
        std::printf("  FAIL: %s\n", what);
        ++failures;
    }
}

bool has_name(const std::vector<std::string>& paths, const std::string& name) {
    for (const std::string& p : paths)
        if (p.size() >= name.size() && p.compare(p.size() - name.size(), name.size(), name) == 0)
            return true;
    return false;
}

void write_file(const std::string& path, const std::string& content) {
    std::FILE* f = platform::open_file(path, "wb");
    if (!f) return;
    std::fwrite(content.data(), 1, content.size(), f);
    // Явный sync: событие «файл изменился» обязано приехать от записи, а не от закрытия буфера
    // в произвольный момент, иначе тест мигает по таймауту.
    platform::sync_file(f);
    std::fclose(f);
}

// Ожидание события. Один poll с большим таймаутом не годится: бэкенды режут пачку по-разному
// (FSEvents агрегирует, inotify отдаёт по событию), и нужный файл может приехать вторым окном.
bool wait_for(platform::Watcher& w, const std::string& name, int attempts) {
    std::vector<std::string> changed;
    for (int i = 0; i < attempts; ++i) {
        if (!w.poll(changed, 500)) return false;
        if (has_name(changed, name)) return true;
    }
    return false;
}

const char* backend_name(platform::WatchBackend b) {
    return b == platform::WatchBackend::Native ? "native" : "poll";
}

void run_case(const std::string& root, platform::WatchBackend expected) {
    const std::string dir = root + "/" + (expected == platform::WatchBackend::Native ? "n" : "p");
    check(platform::ensure_dir(dir), "case dir created");
    const std::string sub = dir + "/sub";

    platform::Watcher w;
    check(w.watch_dir(dir, /*recursive=*/true), "watch_dir succeeds");
    std::printf("  backend: %s%s%s\n", backend_name(w.backend()),
                w.error().empty() ? "" : " - ", w.error().c_str());
    check(w.backend() == expected, "backend is the requested one");

    // 1) Создание файла.
    write_file(dir + "/a.txt", "one");
    check(wait_for(w, "a.txt", 8), "creation of a.txt observed");

    // 2) Правка того же файла: смена содержимого обязана быть видна отдельным событием, иначе
    //    build-loop пересобирал бы только новые файлы.
    write_file(dir + "/a.txt", "one-two-three");
    check(wait_for(w, "a.txt", 8), "modification of a.txt observed");

    // 3) Рекурсия по каталогу, СОЗДАННОМУ после старта наблюдения: у inotify это отдельная
    //    дорегистрация, и без неё новый каталог исходников нем.
    check(platform::ensure_dir(sub), "subdir created");
    write_file(sub + "/b.txt", "child");
    check(wait_for(w, "b.txt", 8), "file in new subdir observed");

    // 4) Тишина: окно без правок — успех с пустым списком, а не отказ.
    std::vector<std::string> changed;
    bool quiet = true;
    for (int i = 0; i < 3 && quiet; ++i) {
        if (!w.poll(changed, 100)) quiet = false;
        if (!changed.empty()) quiet = false;   // хвост прошлых событий — не повод падать сразу
    }
    check(quiet, "quiet window returns success with no paths");

    // 5) timeout_ms=0 — честный опрос без ожидания, а не отказ.
    check(w.poll(changed, 0), "zero timeout polls without waiting");
}

} // namespace

int main() {
    const std::string root =
        platform::exe_dir() + "/likenes_watch_" + std::to_string(platform::process_id());
    if (!platform::ensure_dir(root)) {
        std::printf("platform-watch: cannot create %s\n", root.c_str());
        return 3;
    }

    std::printf("native backend:\n");
    run_case(root, platform::WatchBackend::Native);

    std::printf("forced polling:\n");
    check(platform::env_put("LIKE_NES_WATCH", "poll"), "LIKE_NES_WATCH set");
    run_case(root, platform::WatchBackend::Poll);
    platform::env_put("LIKE_NES_WATCH", nullptr);

    // Наблюдение за несуществующим каталогом — отказ с диагностикой, а не молчаливый watcher,
    // который никогда ничего не отдаст.
    platform::Watcher missing;
    check(!missing.watch_dir(root + "/nope", false), "watch_dir on missing dir fails");
    check(!missing.error().empty(), "failure carries a message");

    const bool pass = failures == 0;
    std::printf("platform-watch: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
