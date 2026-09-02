#include "material_panel.hpp"

#include <cstddef>

#include "../../../engine/asset/hash.hpp"
#include "../compile/diagnostics.hpp"
#include "bake.hpp"
#include "diag.hpp"
#include "gpu.hpp"
#include "imgui.h"
#include "material_frame.hpp"
#include "platform_fs.hpp"

namespace ide::editor {
namespace {

constexpr uint32_t PREVIEW = 256;

// Диагностика идёт в панель ЧЕРЕЗ парсер сборки, а не строкой: click-to-open работает от полей
// (file, line, col), и текст, показанный мимо парсера, выглядел бы в панели так же, но не открывал
// бы файл. Совместимость форматов утверждает `material_diag_compat_test`.
std::string panel_line(const mat::ShaderDiag& d) {
    const std::vector<build::Diagnostic> parsed = build::parse_diagnostics(mat::format_diag(d) + "\n");
    if (parsed.empty()) return mat::format_diag(d);
    const build::Diagnostic& p = parsed.front();
    return p.file + ":" + std::to_string(p.line) + ":" + std::to_string(p.col) + ": " +
           p.severity + ": " + p.message;
}

} // namespace

// Отказ на ЛЮБОМ шаге откатывает всё, что успело подняться: кэш к этому моменту уже создал layout
// и запасной модуль, а сцена — свои буферы, и панель, у которой `init` вернул false, больше никто
// не выключит — `shutdown()` зовётся только у поднявшейся. Та же поломка уже была у игры
// (`MaterialFx::fail`), и второй раз она пишется не по невнимательности, а потому что путь отказа
// здесь свой.
bool MaterialPanel::fail() {
    if (view_) { wgpuTextureViewRelease(view_); view_ = nullptr; }
    if (tex_) { wgpuTextureRelease(tex_); tex_ = nullptr; }
    scene_.shutdown();
    cache_.shutdown();
    table_ = mat::Table{};
    return false;
}

bool MaterialPanel::init(GpuContext& gpu, const std::string& library_dir) {
    gpu_ = &gpu;
    path_ = library_dir + "/sprite_effects.wgsl";
    std::string src;
    if (!platform::read_text(library_dir + "/library.mat", src) ||
        !platform::read_text(path_, wgsl_)) {
        status_ = "library not readable under " + library_dir;
        return fail();
    }
    mat::BakeError err;
    if (!mat::bake_materials(src, table_bytes_, err) ||
        table_.load(table_bytes_.data(), table_bytes_.size()) != mat::LoadResult::Ok) {
        status_ = "library.mat:" + std::to_string(err.line) + ": " + err.message;
        return fail();
    }
    mat::CacheDesc d;
    d.device = gpu.device;
    d.queue = gpu.queue;
    d.target = WGPUTextureFormat_RGBA8Unorm;
    d.table = &table_;
    d.wgsl = wgsl_.c_str();
    if (!cache_.init(d)) { status_ = "cache refused to start"; return fail(); }
    cache_.warm_up();
    if (!scene_.init(gpu.device, gpu.queue, PREVIEW, PREVIEW, cache_.layout())) {
        status_ = "preview scene resources unavailable";
        return fail();
    }
    WGPUTextureDescriptor td = {};
    td.dimension = WGPUTextureDimension_2D;
    td.size = WGPUExtent3D{PREVIEW, PREVIEW, 1};
    td.format = WGPUTextureFormat_RGBA8Unorm;
    td.mipLevelCount = 1;
    td.sampleCount = 1;
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    tex_ = wgpuDeviceCreateTexture(gpu.device, &td);
    view_ = wgpuTextureCreateView(tex_, nullptr);
    ready_ = render_preview();
    if (!ready_) { status_ = "preview did not render"; return fail(); }
    status_ = "watching " + path_;
    if (ready_ && !hot_.start(path_)) status_ = "no watch: " + hot_.error();
    return ready_;
}

bool MaterialPanel::render_preview() {
    scene_.build(table_, cache_);
    const std::vector<uint8_t> px = matgold::render_frame(*gpu_, scene_, PREVIEW, PREVIEW, draws_);
    if (px.size() != static_cast<std::size_t>(PREVIEW) * PREVIEW * 4) return false;
    WGPUImageCopyTexture dst = {};
    dst.texture = tex_;
    dst.aspect = WGPUTextureAspect_All;
    WGPUTextureDataLayout layout = {};
    layout.bytesPerRow = 4 * PREVIEW;
    layout.rowsPerImage = PREVIEW;
    WGPUExtent3D ext = {PREVIEW, PREVIEW, 1};
    wgpuQueueWriteTexture(gpu_->queue, &dst, px.data(), px.size(), &layout, &ext);
    preview_hash_ = asset::fnv1a(px.data(), px.size());
    return true;
}

void MaterialPanel::poll() {
    if (!ready_ || !hot_.watching()) return;
    switch (hot_.poll(cache_, /*timeout_ms=*/0)) {
        case mat::ReloadEvent::Reloaded:
            diag_.clear();
            status_ = render_preview() ? "reloaded" : "reloaded, but the preview did not render";
            break;
        case mat::ReloadEvent::Rejected:
            // Картинка НЕ трогается: на экране остаётся прежний вариант — это и есть утверждение
            // гейта, а не оформление панели.
            diag_ = panel_line(hot_.diag());
            status_ = "rejected, previous library still drawing";
            break;
        case mat::ReloadEvent::None:
            break;
    }
}

void MaterialPanel::draw() {
    if (ImGui::Begin("Shaders")) {
        if (!ready_) {
            ImGui::TextUnformatted(status_.c_str());
            ImGui::End();
            return;
        }
        ImGui::Text("%s", path_.c_str());
        ImGui::Text("%u pipeline(s), %u reload(s), %u rejected, %u draw call(s), %s watch",
                    cache_.pipelines_created(), cache_.reloads(), hot_.rejects(), draws_,
                    hot_.backend() == platform::WatchBackend::Native ? "native" : "polling");
        ImGui::TextUnformatted(status_.c_str());
        if (!diag_.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s", diag_.c_str());
        ImGui::Image(reinterpret_cast<ImTextureID>(view_),
                     ImVec2(static_cast<float>(PREVIEW), static_cast<float>(PREVIEW)));
    }
    ImGui::End();
}

void MaterialPanel::shutdown() {
    if (view_) { wgpuTextureViewRelease(view_); view_ = nullptr; }
    if (tex_) { wgpuTextureRelease(tex_); tex_ = nullptr; }
    scene_.shutdown();
    cache_.shutdown();
    ready_ = false;
}

} // namespace ide::editor
