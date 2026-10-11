#include "section_mode.hpp"

namespace framework::brawl {

namespace {

fix32 before(fix32 v) { return fix32::from_raw(v.raw - 1); }

fix32 gap(fix32 a, fix32 b) { return abs_fix(a - b); }

fix32 nearer(fix32 x, fix32 a, fix32 b) { return gap(x, b) < gap(x, a) ? b : a; }

} // namespace

const scene::Section* section_at(const scene::LevelObjects& level, fix32 x) {
    for (const scene::Section& s : level.sections)
        if (!(x < s.area.rect.x0) && x < s.area.rect.x1) return &s;
    return nullptr;
}

scene::SectionMode mode_at(const scene::LevelObjects& level, fix32 x) {
    const scene::Section* s = section_at(level, x);
    return s != nullptr ? s->mode : scene::SectionMode::Belt;
}

fix32 refusal_x(const scene::LevelObjects& level, fix32 x, scene::SectionMode keep) {
    if (keep == scene::SectionMode::Belt) {
        const scene::Section* here = section_at(level, x);
        if (here == nullptr || here->mode == keep) return x;
        return nearer(x, before(here->area.rect.x0), here->area.rect.x1);
    }
    bool any = false;
    fix32 best = x;
    for (const scene::Section& s : level.sections) {
        if (s.mode != keep || !(s.area.rect.x0 < s.area.rect.x1)) continue;
        const fix32 in = clamp_fix(x, s.area.rect.x0, before(s.area.rect.x1));
        if (!any || gap(x, in) < gap(x, best)) best = in;
        any = true;
    }
    return best;
}

} // namespace framework::brawl
