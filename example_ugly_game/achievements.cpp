#include "achievements.hpp"

#include <cstdio>

#include "../engine/achievements/delivery.hpp"
#include "../engine/achievements/registry.hpp"
#include "../engine/achievements/store.hpp"
#include "../engine/achievements/tracker.hpp"
#include "ach_source.hpp"
#include "backend_host.hpp"

namespace game {
namespace {

constexpr uint32_t TOAST_TICKS = 180;
constexpr uint32_t AUTOSAVE_TICKS = 600;
constexpr int32_t START_LIVES = 3;

const ach::Id STAT_KILLS = ach::hash_key("stat_kills");
const ach::Id STAT_SCORE = ach::hash_key("stat_score");
const ach::Id STAT_DEATHS = ach::hash_key("stat_deaths");
const ach::Id ACH_BOSS_DOWN = ach::hash_key("BOSS_DOWN");
const ach::Id ACH_FLAWLESS = ach::hash_key("FLAWLESS");

// Счётчики GameState обнуляются на каждом новом прогоне (reset_run), а стат достижения —
// пожизненный: наблюдатель шлёт дельту кадра, падение значения читает как начало нового прогона.
uint64_t delta(uint64_t cur, uint64_t& prev) {
    const uint64_t d = cur >= prev ? cur - prev : cur;
    prev = cur;
    return d;
}

void check_key(bool present, const char* key) {
    if (!present) std::fprintf(stderr, "[game] achievements: '%s' is not in the catalogue\n", key);
}

} // namespace

// Порядок полей = порядок жизни: source объявлен первым → разрушается последним, поэтому строки
// реестра (указатели внутрь mmap-региона бандла) валидны всё время жизни reg. delivery объявлен
// последним → умирает первым, до выгрузки плагина в host.
struct Achievements::Impl {
    AchSource source;
    ach::Registry reg;
    std::unique_ptr<ach::Tracker> tracker;
    std::unique_ptr<ach::LocalStore> store;
    BackendHost host;
    std::unique_ptr<ach::Delivery> delivery;
    uint64_t prev_kills = 0;
    uint64_t prev_score = 0;
    uint64_t prev_deaths = 0;
    std::size_t saved_unlocks = 0;
    uint64_t saved_hash = 0;
    uint32_t since_save = 0;
    bool save_failed = false;
};

Achievements::Achievements() : impl_(new Impl()) {}

Achievements::~Achievements() {
    if (impl_->delivery) impl_->delivery->shutdown();
}

// Повторный init разрушал бы Tracker и плагин под живой Delivery (она держит ссылки на оба),
// поэтому второй вызов отвергается: пересоздание — через новый объект.
void Achievements::init(const std::string& bundle_path, const std::string& save_path,
                        const std::string& plugin_path) {
    if (impl_->tracker) {
        std::fprintf(stderr, "[game] achievements: init called twice, ignored\n");
        return;
    }
    if (!impl_->source.open(impl_->reg, bundle_path)) {
        std::fprintf(stderr, "[game] achievements: %s → none\n", impl_->source.reason());
    }
    if (!impl_->reg.entries().empty()) {
        check_key(impl_->reg.find_stat(STAT_KILLS) != nullptr, "stat_kills");
        check_key(impl_->reg.find_stat(STAT_SCORE) != nullptr, "stat_score");
        check_key(impl_->reg.find_stat(STAT_DEATHS) != nullptr, "stat_deaths");
        check_key(impl_->reg.find(ACH_BOSS_DOWN) != nullptr, "BOSS_DOWN");
        check_key(impl_->reg.find(ACH_FLAWLESS) != nullptr, "FLAWLESS");
    }
    impl_->tracker.reset(new ach::Tracker(impl_->reg));
    // saved_unlocks снимается ДО загрузки: restore() ретроактивно открывает достижения, чей порог
    // снимок уже перешагнул, и такие анлоки на диске ещё не лежат — иначе autosave счёл бы их
    // сохранёнными и потерял при крэше.
    impl_->saved_unlocks = impl_->tracker->unlocked_count();
    if (!save_path.empty()) {
        impl_->store.reset(new ach::LocalStore(save_path));
        ach::DecodeResult why = ach::DecodeResult::Ok;
        if (!impl_->store->load(*impl_->tracker, &why) && why != ach::DecodeResult::Missing) {
            std::fprintf(stderr, "[game] achievements: snapshot rejected (%s) → starting empty\n",
                         ach::decode_reason(why));
        }
        const std::size_t kept = impl_->tracker->carried_count();
        if (kept != 0) {
            std::fprintf(stderr,
                         "[game] achievements: %zu snapshot records outside the catalogue, "
                         "kept as-is\n",
                         kept);
        }
    }
    impl_->saved_hash = impl_->tracker->progress_hash();
    backend_ = impl_->host.load(plugin_path);
    if (backend_ == nullptr) return;
    impl_->delivery.reset(new ach::Delivery(impl_->reg, *impl_->tracker, *backend_));
}

void Achievements::observe(const GameState& gs) {
    if (!impl_->tracker) return;
    ach::Tracker& tr = *impl_->tracker;
    tr.set_tick(gs.tick);
    const uint64_t deaths =
        gs.lives < START_LIVES ? static_cast<uint64_t>(START_LIVES - gs.lives) : 0;
    const uint64_t d_kills = delta(gs.kills, impl_->prev_kills);
    const uint64_t d_score = delta(gs.score, impl_->prev_score);
    const uint64_t d_deaths = delta(deaths, impl_->prev_deaths);
    tr.add_stat(STAT_KILLS, d_kills);
    tr.add_stat(STAT_SCORE, d_score);
    tr.add_stat(STAT_DEATHS, d_deaths);
    if (gs.phase == PH_Victory) {
        tr.unlock(ACH_BOSS_DOWN);
        if (gs.lives >= START_LIVES) tr.unlock(ACH_FLAWLESS);
    }

    if (toast_.left > 0) --toast_.left;
    if (toast_.left == 0 && !tr.events().empty()) {
        const ach::Entry* e = impl_->reg.find(tr.events().front().id);
        toast_.name = e != nullptr ? e->name : "Achievement";
        toast_.left = TOAST_TICKS;
        tr.drain(1);
    }
}

void Achievements::pump() {
    if (!impl_->delivery) return;
    impl_->delivery->pump();
    impl_->delivery->reconcile();
}

// Неудачная запись не должна выглядеть как успешная: состояние «на диске» не двигается, попытка
// повторится, но не каждый кадр — since_save считает тики с последней ПОПЫТКИ, а не с успеха.
void Achievements::save() {
    if (!impl_->store) return;
    impl_->since_save = 0;
    if (!impl_->store->save(*impl_->tracker)) {
        if (!impl_->save_failed) {
            std::fprintf(stderr, "[game] achievements: cannot write the snapshot, progress stays in memory\n");
        }
        impl_->save_failed = true;
        return;
    }
    impl_->save_failed = false;
    impl_->saved_unlocks = impl_->tracker->unlocked_count();
    impl_->saved_hash = impl_->tracker->progress_hash();
}

// Крэш не должен стоить сессии: анлок кладём на диск сразу, накопленные статы — раз в AUTOSAVE_TICKS.
// «Есть что писать» считается по состоянию трекера, а не по дельтам GameState: стат, которого нет в
// каталоге, дельту даёт, а снимок не меняет — и автосейв писал бы идентичный файл каждые 600 тиков.
void Achievements::autosave() {
    if (!impl_->store) return;
    ++impl_->since_save;
    const bool unlocks_pending = impl_->tracker->unlocked_count() != impl_->saved_unlocks;
    const bool changed = impl_->tracker->progress_hash() != impl_->saved_hash;
    if (!unlocks_pending && !changed) return;
    if (unlocks_pending && !impl_->save_failed) {
        save();
        return;
    }
    if (impl_->since_save < AUTOSAVE_TICKS) return;
    save();
}

std::size_t Achievements::unlocked_count() const {
    return impl_->tracker ? impl_->tracker->unlocked_count() : 0;
}

std::size_t Achievements::defined_count() const { return impl_->reg.entries().size(); }

} // namespace game
