#pragma once
#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>
#include <string>

struct GpuContext;

namespace mobile {

// Отступы безопасной зоны — в точках; scale — пикселей кадра на точку (fb = точки × scale).
struct SafeArea { float left = 0, top = 0, right = 0, bottom = 0, scale = 1; };
enum class Touch { Down, Move, Up };

// Игра на телефоне глазами оболочки. Жизнь игры и жизнь поверхности разведены: Android снимает
// окно на каждом уходе в фон (APP_CMD_TERM_WINDOW), iOS — поверхность при уходе в фон, а процесс и
// состояние игры остаются. Поэтому `init` и `shutdown` — ровно по разу на объект, а
// `attach_surface`/`detach_surface` — сколько угодно раз между ними. Объект живёт, пока живы
// активность Android или сцена iOS: Android может войти в android_main повторно в том же процессе,
// и тогда make_game() даёт новый объект. Поверхностью владеет оболочка: игра её конфигурирует и
// рисует в неё, но не освобождает. Размер кадра — в пикселях, нулевой бывает до первой раскладки.
//
// `suspend`/`resume` — о жизни процесса на переднем плане, а не о поверхности: Android шлёт
// APP_CMD_RESUME раньше нового окна, iOS — после, и порядок относительно attach/detach игре не
// обещан. Потеря устройства не лечится уходом в фон: оболочка GPU не пересоздаёт, игра перестаёт
// просить кадры (`wants_frames`), и вернуть картинку может только перезапуск.
class Game {
public:
    virtual ~Game() = default;
    virtual bool init(GpuContext& gpu) = 0;
    virtual bool attach_surface(WGPUSurface surface, uint32_t fb_w, uint32_t fb_h, const SafeArea& safe) = 0;
    virtual void detach_surface() = 0;
    virtual void resize(uint32_t fb_w, uint32_t fb_h, const SafeArea& safe) = 0;
    // id — intptr_t: iOS отдаёт адрес UITouch целиком, усечение до int склеило бы два касания.
    virtual void pointer(intptr_t id, Touch phase, float x_pt, float y_pt) = 0;
    // Системный CANCEL (шторка, звонок): сбросить все активные касания.
    virtual void cancel() = 0;
    virtual void frame(double dt) = 0;
    // Нужен ли игре следующий кадр. false — оболочка перестаёт будить петлю (Android ждёт событий
    // без таймаута, iOS ставит CADisplayLink на паузу) до следующего attach_surface: бенчу кадр не
    // нужен вовсе, а игре без поверхности или с потерянным устройством рисовать некуда.
    virtual bool wants_frames() const = 0;
    // Фоновый процесс система убивает без dealloc и onDestroy: всё, что должно пережить его,
    // сохраняется здесь.
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual void shutdown() = 0;
    // Хвост строки `[touch]` пробы iOS-гейта (кто держит стик и огонь): маршрутизацию касаний
    // знает игра, а печатает строку оболочка. Пусто — проба игре не нужна.
    virtual std::string touch_state() const { return {}; }
};

// Определяет игра: оболочка не знает, какую игру она запускает.
std::unique_ptr<Game> make_game();

} // namespace mobile
