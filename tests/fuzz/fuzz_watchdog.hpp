#pragma once
// Сторож зависания гейта 9. Отдельным файлом, потому что это отдельная ответственность: драйвер
// РАЗДАЁТ случаи и считает статистику, а здесь живёт единственный вопрос — вернулся ли читатель.
//
// Замер вокруг вызова (`started` до, `spent` после) ловит только МЕДЛЕННЫЙ разбор: чтобы его
// сравнить с порогом, вызов должен сперва вернуться. Цикл, который не сходится, не возвращается
// никогда, и без сторожа исход ровно такой, какой гейт обещал заменить: 60-минутный таймаут задачи
// без имени цели, seed'а и номера случая — а на машине владельца и таймаута нет, preflight просто
// висит. Поэтому дедлайн проверяет ВТОРОЙ поток, которому застрявший читатель не мешает.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace fuzz {
namespace watchdog {

inline std::atomic<long long> g_deadline_ns{0};  // 0 — сторож разоружён, случай не исполняется
inline std::atomic<const char*> g_target{nullptr};
inline std::atomic<unsigned long long> g_seed{0};
inline std::atomic<int> g_index{0};
// Поколение случая — счётчик seqlock'а, и младший бит у него значащий. ЧЁТНОЕ значит «описание
// цело», НЕЧЁТНОЕ — «главный поток прямо сейчас внутри `arm`». Тождество случая нельзя судить
// одним дедлайном: пока `arm` дописывает описание, срок ещё ПРЕЖНИЙ, и сторож с истёкшим прежним
// сроком на руках назвал бы случай, который даже не начинал читаться, — команда повтора не
// повторила бы такой прогон, потому что повторять нечего. Растёт счётчик дважды за вооружение
// (до и после записи описания) и на два при разоружении, а сторож печатает, только если он не
// сдвинулся и был чётным.
inline std::atomic<unsigned long long> g_gen{0};

inline long long now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

inline void arm(const char* target, unsigned long long seed, int index, double budget_sec) {
    // Нечётное поколение поднимается ПЕРЕД записью описания: это пишущая половина seqlock'а, и
    // без неё читающая ничего не стоит. Сторож, проснувшийся между записью описания и записью
    // срока, увидел бы НОВОЕ имя при ПРЕЖНЕМ сроке — перепроверка срока такую полузапись
    // пропускает, потому что срок ещё не менялся.
    g_gen.fetch_add(1, std::memory_order_relaxed);
    // Барьер, а не release на самом инкременте: release упорядочивает то, что было ДО него, и
    // ничем не мешает опустить записи описания НИЖЕ публикации нечётного счётчика. Тогда сторож
    // увидел бы чётное поколение при уже наполовину новом описании — ровно то, от чего бит и
    // заводился.
    std::atomic_thread_fence(std::memory_order_release);
    g_target.store(target, std::memory_order_relaxed);
    g_seed.store(seed, std::memory_order_relaxed);
    g_index.store(index, std::memory_order_relaxed);
    g_deadline_ns.store(now_ns() + static_cast<long long>(budget_sec * 1e9),
                        std::memory_order_relaxed);
    // Поколение пишется ПОСЛЕДНИМ и с release, а читается ПЕРВЫМ и с acquire: увидев чётное
    // значение, сторож уже видит и описание, и срок, положенные до него. Release на самом
    // дедлайне для этого мал — он публикует описание, но не мешает перевооружить случай сразу
    // после снимка срока.
    g_gen.fetch_add(1, std::memory_order_release);
}

inline void disarm() {
    g_deadline_ns.store(0, std::memory_order_relaxed);
    // Плюс ДВА, а не один: младший бит занят признаком «запись идёт», а разоружение описания не
    // трогает. Оставь оно поколение нечётным — сторож замолчал бы до конца прогона.
    g_gen.fetch_add(2, std::memory_order_release);
}

// Дисковое время случая (см. `fuzz_io_span.hpp`) сторож не судит вовсе, и это ПАУЗА, а не сдвиг
// задним числом. Сдвигать дедлайн в конце куска поздно по построению: срок истекает ВНУТРИ
// заминки, то есть ровно в том событии, ради которого вычет и делался, — антивирус на закрытии
// файла даёт «the reader never returned» на живом читателе. Поэтому вход в дисковый кусок
// поднимает счётчик, и пока он ненулевой, сторож молчит; выход опускает счётчик и сдвигает дедлайн
// на измеренное, чтобы сторож ждал столько же, сколько ждёт порог.
//
// Цена названа прямо: ЗАВИСАНИЕ ВНУТРИ дискового вызова сторож не ловит. Под паузой живут только
// `open_file`/`fwrite`/`fclose`/`remove_file` — шов платформы, а не читатель; зависшая там
// файловая система — событие ОС, и гейт не берётся судить её тем же текстом, которым судит
// читателя. Разбор самого манифеста (`parse_manifest`) под паузу НЕ попадает: он и есть предмет.
inline std::atomic<int> g_io_depth{0};

inline void io_enter() { g_io_depth.fetch_add(1, std::memory_order_acq_rel); }

inline void io_leave(double sec) {
    if (g_deadline_ns.load(std::memory_order_acquire) != 0) {
        g_deadline_ns.fetch_add(static_cast<long long>(sec * 1e9), std::memory_order_acq_rel);
    }
    g_io_depth.fetch_sub(1, std::memory_order_acq_rel);
}

// Запас поверх порога случая — НЕ вкусовщина: вердикт о медленном разборе обязан оставаться за
// замером в цикле, который называет измеренные секунды. Сторож — про «не вернулся вовсе», и если
// бы он срабатывал на том же пороге, два разных отказа гонялись бы наперегонки и мутант «порог
// равен нулю» краснел бы то одним текстом, то другим.
inline void start(double poll_sec) {
    std::thread([poll_sec] {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::duration<double>(poll_sec));
            const unsigned long long gen = g_gen.load(std::memory_order_acquire);
            const long long deadline = g_deadline_ns.load(std::memory_order_relaxed);
            if (deadline == 0 || now_ns() <= deadline) continue;
            // Нечётное поколение — главный поток ВНУТРИ `arm`: описание уже новое, а срок ещё
            // прежний, и перепроверка срока ниже такую полузапись пропустит, потому что срок не
            // менялся. Читающая половина seqlock'а начинается ИМЕННО здесь, а не на перепроверке.
            if ((gen & 1ull) != 0) continue;
            // Пауза читается ПОСЛЕ срока, но ОДНОЙ ЕЁ МАЛО: между снимком срока и этой строкой
            // `io_leave` успевает и продлить дедлайн, и опустить счётчик до нуля — сторож увидел бы
            // ноль и обвинил ЖИВОГО читателя, только что вернувшегося из медленного дискового
            // вызова. Ноль здесь — лишь повод смотреть дальше; решает перепроверка снимка ниже.
            if (g_io_depth.load(std::memory_order_acquire) != 0) continue;
            const char* name = g_target.load(std::memory_order_relaxed);
            const unsigned long long seed = g_seed.load(std::memory_order_relaxed);
            const int index = g_index.load(std::memory_order_relaxed);
            // Барьер, а не просто acquire-чтение: иначе компилятору вольно опустить три relaxed
            // чтения описания НИЖЕ перепроверки, и они прочитали бы уже следующий случай. Это
            // читающая половина seqlock'а, и поколение здесь работает ровно как его счётчик.
            std::atomic_thread_fence(std::memory_order_acquire);
            // Поколение сдвинулось — случай успели разоружить, перевооружить или начать писать,
            // вердикт назвал бы не тот случай. Дедлайн изменился при ТОМ ЖЕ поколении — его
            // продлил `io_leave`, то есть срок истёк внутри дисковой паузы и читатель жив. В
            // обоих исходах вердикта нет.
            if (g_gen.load(std::memory_order_relaxed) != gen) continue;
            if (g_deadline_ns.load(std::memory_order_relaxed) != deadline) continue;
            std::fprintf(stderr, "[fuzz] FAIL %s seed=%llu case=%d: the reader never returned\n",
                         name == nullptr ? "?" : name, seed, index);
            std::fflush(stderr);
            // stdout сбрасывается ЯВНО, а не в расчёте на режим буфера: `main` снимает буфер
            // вовсе, но отказ `setvbuf` не проверяется, и эта строка закрывает такой исход.
            std::fflush(stdout);
            // `_Exit`, а не `exit` и не `abort`: главный поток застрял ВНУТРИ читателя, с
            // произвольным состоянием на руках. Статические деструкторы из чужого потока в такой
            // момент — второй отказ поверх первого, а проверка утечек ASan при выходе сама умеет
            // ждать застрявший поток. Сообщение уже во flush'енном stderr, а вердикты целей — в
            // stdout: их уносит явный `fflush` после каждой строки в `main`, а не буферизация.
            std::_Exit(1);
        }
    }).detach();
}

} // namespace watchdog
} // namespace fuzz
