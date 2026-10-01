# Публичные заголовки SDK — явным списком, без glob: новый публичный заголовок — решение, а не
# случайность (спека #24, В1). Пути относительно engine/, ставятся в include/like-nes/<каталог>.
# Внутренние (renderer_internal, shaders*, gpu_util, win32_*, platform_capture, platform_net),
# тестовые (*_scene, *probe*, *_lcg, *_load, *_observed, *_toy) и заголовки целей вне SDK сюда не
# входят; замкнутость набора по #include проверяет install_sdk.cmake при конфигурировании.

set(LIKE_NES_SDK_HEADERS
  core/fixed.hpp
  asset/hash.hpp
  platform/platform_args.hpp platform/platform_env.hpp platform/platform_export.h
  platform/platform_fs.hpp platform/platform_guard.hpp platform/platform_io.hpp
  platform/platform_module.hpp platform/platform_noinline.hpp platform/platform_path.hpp
  platform/platform_process.hpp platform/platform_redact.hpp platform/platform_shmem.hpp
  platform/platform_watch.hpp
  input/action_map.hpp input/codes.hpp input/device_state.hpp input/input_buffer.hpp
  input/input_engine.hpp input/input_sim.hpp input/input_spsc.hpp input/input_types.hpp
  input/source.hpp
  render/arena.hpp render/gpu.hpp render/render_capture.hpp render/render_sprite.hpp
  render/surface_frame.hpp
  framework/core/fixmath.hpp framework/core/fixtrig.hpp framework/core/schedule.hpp
  framework/core/stage.hpp framework/core/text_fields.hpp
  framework/input/pad_profile.hpp framework/input/pad_registry.hpp
  framework/input/preset_axes.hpp framework/input/preset_bake.hpp
  framework/input/preset_format.hpp framework/input/preset_parse.hpp
  framework/input/presets.hpp framework/input/rebind_session.hpp
  framework/input/rebind_store.hpp framework/input/source_names.hpp
  framework/input/stick.hpp
  framework/physics/axis_terms.hpp framework/physics/body.hpp framework/physics/broadphase.hpp
  framework/physics/cast.hpp framework/physics/contact.hpp framework/physics/counters.hpp
  framework/physics/distance.hpp framework/physics/event_hash.hpp framework/physics/events.hpp
  framework/physics/filter.hpp framework/physics/gap.hpp framework/physics/hash_mix.hpp
  framework/physics/impulse.hpp framework/physics/island.hpp framework/physics/mass.hpp
  framework/physics/narrowphase.hpp framework/physics/nearest.hpp
  framework/physics/physics_cache.hpp framework/physics/physics_query.hpp
  framework/physics/physics_world.hpp framework/physics/prepare.hpp
  framework/physics/query_index.hpp framework/physics/rest.hpp framework/physics/sat.hpp
  framework/physics/shape.hpp framework/physics/snapshot.hpp framework/physics/solver.hpp
  framework/physics/state_hash.hpp framework/physics/sweep_order.hpp framework/physics/units.hpp
  framework/tilemap/grid.hpp framework/tilemap/map_bake.hpp framework/tilemap/map_format.hpp
  framework/tilemap/map_read.hpp framework/tilemap/tile_rules.hpp
  framework/tilemap/tile_shape.hpp framework/tilemap/tilemap_query.hpp
  framework/character/assist.hpp framework/character/character_state.hpp
  framework/character/collision.hpp framework/character/controller.hpp
  framework/character/ladder.hpp framework/character/profile.hpp
  framework/character/profile_bake.hpp framework/character/profile_format.hpp
  framework/character/profile_read.hpp framework/character/push.hpp
  framework/character/slide.hpp framework/character/support.hpp
  framework/character/trajectory.hpp
  framework/graphics/atlas_bake.hpp framework/graphics/atlas_format.hpp
  framework/graphics/atlas_read.hpp framework/graphics/camera.hpp framework/graphics/clip.hpp
  framework/graphics/debug_draw.hpp framework/graphics/graphics_sprite.hpp
  framework/graphics/machine.hpp framework/graphics/nine_slice.hpp
  framework/graphics/particles.hpp framework/graphics/player.hpp
  framework/graphics/tile_draw.hpp framework/graphics/viewport.hpp
  framework/rollback/input_ring.hpp framework/rollback/plan.hpp framework/rollback/session.hpp
  framework/replay/stream.hpp framework/replay/verify.hpp
)

# Статические цели SDK. Порядок не важен: граф ссылок Config выводит из целей дерева.
set(LIKE_NES_SDK_STATIC
  framework_core framework_input framework_physics framework_tilemap framework_character
  framework_graphics framework_graphics_tiles framework_rollback
  input_core platform_core render_core render_surface glfw glfw3webgpu)
set(LIKE_NES_SDK_INTERFACE engine_core asset_hash framework_replay)
# То, что видит игра под like-nes::engine; окно — отдельно, like-nes::window.
set(LIKE_NES_SDK_ENGINE
  engine_core asset_hash platform_core input_core render_core render_surface
  framework_core framework_input framework_physics framework_tilemap framework_character
  framework_graphics framework_graphics_tiles framework_rollback framework_replay)
