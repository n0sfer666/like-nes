# Публичные заголовки SDK — явным списком, без glob: новый публичный заголовок — решение, а не
# случайность (спека #24, В1). Пути относительно engine/, ставятся в include/like-nes/<каталог>.
# Внутренние (renderer_internal, shaders*, gpu_util, win32_*, platform_capture, platform_net),
# тестовые (*_scene, *probe*, *_lcg, *_load, *_observed, *_toy) и заголовки целей вне SDK сюда не
# входят; замкнутость набора по #include проверяет install_sdk.cmake при конфигурировании.

set(LIKE_NES_SDK_HEADERS
  core/fixed.hpp core/sim_tick.hpp
  audio/audio_types.hpp audio/audio_spsc.hpp audio/fix_math.hpp audio/limiter.hpp audio/voice.hpp
  audio/mixer.hpp audio/audio_engine.hpp audio/decoder.hpp audio/vorbis_reader.hpp
  audio/audio_stereo_ring.hpp
  audio/music_intent.hpp
  audio/music_track.hpp audio/music_stats.hpp audio/music_stream.hpp
  audio/device.hpp
  asset/hash.hpp asset/format.hpp asset/bundle_view.hpp asset/bundle_lookup.hpp
  platform/platform_args.hpp platform/platform_env.hpp platform/platform_export.h
  platform/platform_fs.hpp platform/platform_guard.hpp platform/platform_io.hpp platform/platform_haptic.hpp platform/platform_save.hpp
  platform/platform_module.hpp platform/platform_noinline.hpp platform/platform_path.hpp
  platform/platform_process.hpp platform/platform_redact.hpp platform/platform_shmem.hpp
  platform/platform_watch.hpp
  input/action_layout.hpp input/action_map.hpp input/action_shared.hpp input/codes.hpp
  input/device_state.hpp input/input_buffer.hpp input/input_engine.hpp input/input_sim.hpp
  input/input_spsc.hpp input/input_types.hpp input/player_assign.hpp input/source.hpp
  render/arena.hpp render/gpu.hpp render/render_capture.hpp render/render_sprite.hpp
  render/surface_frame.hpp render/quad_batch.hpp render/quad_types.hpp
  framework/core/fixmath.hpp framework/core/fixtrig.hpp framework/core/schedule.hpp
  framework/core/stage.hpp framework/core/text_fields.hpp framework/core/utf8_decode.hpp
  framework/core/credits_format.hpp framework/core/credits_read.hpp framework/core/credits_bake.hpp
  framework/core/ent_id.hpp framework/core/world_rng.hpp framework/core/seats.hpp
  framework/input/pad_profile.hpp framework/input/pad_registry.hpp framework/input/player_layout.hpp
  framework/input/preset_axes.hpp framework/input/preset_bake.hpp
  framework/input/preset_format.hpp framework/input/preset_parse.hpp
  framework/input/presets.hpp framework/input/profile_file.hpp framework/input/rebind_session.hpp
  framework/input/rebind_store.hpp framework/input/source_names.hpp
  framework/input/stick.hpp framework/input/seat_state.hpp framework/input/layout_keys.hpp
  framework/input/lobby.hpp framework/input/flick.hpp framework/input/action_buffer.hpp
  framework/physics/axis_terms.hpp framework/physics/body.hpp framework/physics/broadphase.hpp
  framework/physics/cast.hpp framework/physics/contact.hpp framework/physics/counters.hpp
  framework/physics/distance.hpp framework/physics/event_hash.hpp framework/physics/events.hpp
  framework/physics/filter.hpp framework/physics/gap.hpp framework/physics/hash_mix.hpp
  framework/physics/impulse.hpp framework/physics/island.hpp framework/physics/mass.hpp
  framework/physics/narrowphase.hpp framework/physics/nearest.hpp framework/physics/pendulum.hpp
  framework/physics/physics_cache.hpp framework/physics/physics_query.hpp
  framework/physics/physics_world.hpp framework/physics/prepare.hpp
  framework/physics/query_index.hpp framework/physics/rest.hpp framework/physics/sat.hpp
  framework/physics/seat_hash.hpp framework/physics/shape.hpp framework/physics/snapshot.hpp
  framework/physics/solver.hpp framework/physics/state_hash.hpp framework/physics/sweep_order.hpp
  framework/physics/units.hpp
  framework/depth/depth_body.hpp framework/depth/depth_profile.hpp framework/depth/depth_floor.hpp
  framework/depth/depth_move.hpp framework/depth/belt_ride.hpp
  framework/hits/hit_box.hpp framework/hits/struck_list.hpp framework/hits/hitstop.hpp
  framework/kinematic/path_follow.hpp framework/kinematic/movers.hpp
  framework/kinematic/movers_snapshot.hpp framework/kinematic/movers_hash.hpp
  framework/tilemap/grid.hpp framework/tilemap/map_bake.hpp framework/tilemap/map_format.hpp
  framework/tilemap/map_read.hpp framework/tilemap/tile_rules.hpp
  framework/tilemap/tile_shape.hpp framework/tilemap/tilemap_query.hpp
  framework/core/section_bake.hpp framework/core/section_format.hpp framework/core/section_open.hpp
  framework/tilemap/visual_format.hpp framework/tilemap/visual_read.hpp
  framework/tilemap/visual_texels.hpp framework/tilemap/object_format.hpp
  framework/tilemap/object_read.hpp
  framework/character/character_state.hpp framework/character/collision.hpp
  framework/character/push.hpp framework/character/slide.hpp framework/character/support.hpp
  framework/graphics/atlas_bake.hpp framework/graphics/atlas_format.hpp
  framework/graphics/atlas_read.hpp framework/graphics/camera.hpp framework/graphics/camera_autoscroll.hpp framework/graphics/clip.hpp
  framework/graphics/debug_draw.hpp framework/graphics/graphics_sprite.hpp
  framework/graphics/machine.hpp framework/graphics/nine_slice.hpp
  framework/graphics/particles.hpp framework/graphics/player.hpp
  framework/graphics/tile_draw.hpp framework/graphics/viewport.hpp framework/graphics/viewport_fit.hpp framework/graphics/viewport_settings.hpp
  framework/graphics/layer_draw.hpp framework/graphics/gpu/layer_quads.hpp
  framework/graphics/sprite_flip.hpp framework/graphics/clip_format.hpp
  framework/graphics/clip_read.hpp framework/graphics/clip_bake.hpp
  framework/graphics/clip_debug.hpp framework/graphics/gpu/cel_quads.hpp
  framework/graphics/font_format.hpp framework/graphics/font_read.hpp framework/graphics/font_bake.hpp
  framework/graphics/text_layout.hpp framework/graphics/gpu/text_quads.hpp
  framework/rollback/input_ring.hpp framework/rollback/plan.hpp framework/rollback/session.hpp
  framework/rollback/rollback_timing.hpp
  framework/haptic/haptic.hpp
  framework/ui/strings.hpp framework/ui/strings_format.hpp framework/ui/strings_bake.hpp
  framework/ui/ui_input.hpp
  framework/ui/ui_context.hpp
  framework/music/music_format.hpp framework/music/music_read.hpp framework/music/music_bake.hpp
  framework/save/save.hpp framework/save/save_format.hpp framework/save/save_backend.hpp framework/save/save_slots.hpp
  framework/touch/touch_layout.hpp framework/touch/touch_format.hpp framework/touch/touch_source.hpp
  framework/replay/stream.hpp framework/replay/verify.hpp
  framework/scene/scene_paths.hpp framework/scene/scene_objects.hpp framework/scene/scene_read.hpp framework/scene/scene_object.hpp framework/scene/scene_stack.hpp
  framework/scene/viewport_load.hpp framework/scene/scene_level.hpp framework/scene/level_error.hpp framework/scene/level_load.hpp
)

# Статические цели SDK. Порядок не важен: граф ссылок Config выводит из целей дерева.
set(LIKE_NES_SDK_STATIC
  audio_core
  framework_save framework_ui framework_music framework_haptic platform_haptic framework_core framework_input framework_touch framework_physics framework_depth framework_hits framework_kinematic framework_tilemap framework_character
  framework_graphics framework_graphics_tiles framework_graphics_gpu framework_rollback
  framework_scene asset_view input_core input_native input_gamepad platform_core render_core
  render_surface glfw glfw3webgpu)
if(TARGET audio_device)
  list(APPEND LIKE_NES_SDK_STATIC audio_device)
endif()
set(LIKE_NES_SDK_INTERFACE engine_core asset_hash framework_replay render_types)
# То, что видит игра под like-nes::engine; окно и нативный ввод — отдельно, like-nes::window.
set(LIKE_NES_SDK_ENGINE
  audio_core
  framework_save framework_ui framework_music engine_core asset_hash asset_view platform_core input_core render_core render_surface
  framework_haptic framework_core framework_input framework_touch framework_physics framework_depth framework_hits framework_kinematic framework_tilemap framework_character
  framework_graphics framework_graphics_tiles framework_graphics_gpu framework_rollback
  framework_replay framework_scene)
