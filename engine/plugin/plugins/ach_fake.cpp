#include "../plugin_api.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Фейковый бэкенд достижений: повторяет контракт Steamworks (unlock/stat/commit/callback)
// без SDK. Управляется переменными окружения — CI гоняет шов, не трогая Steam.
//   ACH_FAKE_OFFLINE=N   — первые N вызовов begin() возвращают «недоступен»
//   ACH_FAKE_RETRY=N     — первые N отправок возвращают RETRY
//   ACH_FAKE_FATAL=1     — первая отправка возвращает FATAL
//   ACH_FAKE_REMOTE=A,B  — ключи, «уже открытые» на стороне сервиса
//   ACH_FAKE_REMOTE_LATE=K — ключ, открытый на сервисе уже ПОСЛЕ первого опроса (оверлей, вторая
//                            машина): доезжает со второго poll_remote
namespace {

struct Fake {
    int offline = 0;
    int retry = 0;
    bool fatal = false;
    int polls = 0;
    std::string late;
    std::vector<std::string> unlocked;
    std::vector<std::string> remote;
};

Fake g_fake;

int env_int(const char* name) {
    const char* v = std::getenv(name);
    return v == nullptr ? 0 : std::atoi(v);
}

void split_remote(const char* v) {
    std::string cur;
    for (const char* p = v; ; ++p) {
        if (*p == ',' || *p == '\0') {
            if (!cur.empty()) g_fake.remote.push_back(cur);
            cur.clear();
            if (*p == '\0') break;
            continue;
        }
        cur.push_back(*p);
    }
}

int32_t fake_begin(void* self) {
    Fake* f = static_cast<Fake*>(self);
    if (f->offline > 0) {
        --f->offline;
        return 0;
    }
    return 1;
}

void fake_declare(void*, const char*) {}

int32_t gate(Fake* f) {
    if (f->fatal) {
        f->fatal = false;
        return ACH_SEND_FATAL;
    }
    if (f->retry > 0) {
        --f->retry;
        return ACH_SEND_RETRY;
    }
    return ACH_SEND_OK;
}

int32_t fake_unlock(void* self, const char* key) {
    Fake* f = static_cast<Fake*>(self);
    const int32_t r = gate(f);
    if (r != ACH_SEND_OK) return r;
    for (const std::string& k : f->unlocked) {
        if (k == key) return ACH_SEND_OK;
    }
    f->unlocked.push_back(key);
    return ACH_SEND_OK;
}

int32_t fake_set_stat(void* self, const char* key, uint64_t value) {
    Fake* f = static_cast<Fake*>(self);
    const int32_t r = gate(f);
    if (r != ACH_SEND_OK) return r;
    std::printf("[ach-fake] stat %s = %llu\n", key, static_cast<unsigned long long>(value));
    return ACH_SEND_OK;
}

int32_t fake_commit(void*) { return ACH_SEND_OK; }

// Полный текущий набор на каждый вызов, без защёлки: сверка периодическая, и «отдал один раз»
// означало бы, что удалённый анлок посреди сессии не подхватится никогда.
int32_t fake_poll_remote(void* self, const char** out_keys, int32_t cap) {
    Fake* f = static_cast<Fake*>(self);
    // Поздний ключ дописывается ДО заполнения out_keys: реаллокация remote после неё оборвала бы
    // уже отданные указатели, которые контракт обязывает держать валидными до следующего вызова.
    if (!f->late.empty() && f->polls >= 1) {
        f->remote.push_back(f->late);
        f->late.clear();
    }
    ++f->polls;
    int32_t n = 0;
    for (const std::string& k : f->remote) {
        if (n >= cap) break;
        out_keys[n++] = k.c_str();
    }
    return n;
}

void fake_end(void*) {}

AchBackendApi g_api = {.self = &g_fake,
                       .begin = fake_begin,
                       .declare = fake_declare,
                       .unlock = fake_unlock,
                       .set_stat = fake_set_stat,
                       .commit = fake_commit,
                       .poll_remote = fake_poll_remote,
                       .end = fake_end};

} // namespace

PLUGIN_EXPORT_ABI

extern "C" void plugin_main(const HostApi* host) {
    // Полный сброс: при повторной загрузке (BackendHost::load второй раз) dlclose мог не размапить
    // библиотеку, и уцелевшее состояние прошлого прогона — дописанный remote, уже открытые
    // ачивки — молча переехало бы в новый.
    g_fake = Fake{};
    g_fake.offline = env_int("ACH_FAKE_OFFLINE");
    g_fake.retry = env_int("ACH_FAKE_RETRY");
    g_fake.fatal = env_int("ACH_FAKE_FATAL") != 0;
    const char* remote = std::getenv("ACH_FAKE_REMOTE");
    if (remote != nullptr) split_remote(remote);
    const char* late = std::getenv("ACH_FAKE_REMOTE_LATE");
    if (late != nullptr) g_fake.late = late;
    host->register_achievement_backend(host->ctx, "fake", &g_api);
    host->log(host->ctx, "ach-fake backend registered");
}
