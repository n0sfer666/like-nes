<!-- en-sha256: 5b044aeb7bbb894ab6a3d059a0cc3184580a96d0892944a0190cd53b334759ef -->

# Детерминизм и арифметика с фиксированной точкой

[English](../../en/guide/determinism.md) · Русский

Движок обещает, что одинаковый ввод даёт одинаковую симуляцию на любой машине, под которую он
собирается. На этом обещании стоят реплеи, сетевая модель и половина собственных гейтов проекта —
поэтому стоит точно сказать, чем оно держится и что его ломает.

Всё показанное ниже взято из [`docs/examples/fixed_point.cpp`](../../examples/fixed_point.cpp),
который CI собирает и запускает на трёх операционных системах.

## Арифметика симуляции — Q16.16, а не плавающая точка

`fix32` — 32-разрядное знаковое число с фиксированной точкой и 16 дробными битами. Диапазон
примерно ±32768, шаг 1/65536. Любое значение, доезжающее до хеша симуляции, — такое число.

Плавающая точка запрещена не по вкусу. Она запрещена потому, что одно и то же выражение ВПРАВЕ дать
разный результат на разных машинах: x87 держит 80-битные промежуточные значения, слитое
умножение-сложение округляет один раз там, где две операции округлили бы дважды, а компилятор под
оптимизацией свободно переассоциирует. Любое из трёх превращает «тот же реплей» в «почти тот же»,
а это то же самое, что сломанный.

<!-- snippet: docs/examples/fixed_point.cpp#basics -->
```cpp
// `from_int` is exact. `from_float` rounds to the nearest representable value and is meant for
// authoring constants, not for the hot path: it is the only door where a double gets in.
const fix32 three = fix32::from_int(3);
const fix32 half = fix32::from_float(0.5);
show("3 * 0.5", three * half);
show("1 / 3", fix32::from_int(1) / three);
show("0.1 + 0.2", fix32::from_float(0.1) + fix32::from_float(0.2));
```
<!-- /snippet -->

Обратите внимание, что печатается, а что нет:

<!-- snippet: docs/examples/fixed_point.cpp#print -->
```cpp
// Only the raw bits are printed, never the double. `to_double` exists for rendering and for reading
// a value in a debugger; comparing simulation results through it would compare the printf of the
// platform's libc, not the engine.
static void show(const char* what, fix32 v) {
    std::printf("%-28s raw 0x%08x\n", what, static_cast<unsigned>(v.raw));
}
```
<!-- /snippet -->

## Выход за диапазон насыщает: не заворачивает и не падает

<!-- snippet: docs/examples/fixed_point.cpp#saturation -->
```cpp
// Overflow saturates at the ends of the range instead of wrapping, and division by zero
// saturates too. Both are deliberate: a wrapped coordinate is a character teleporting across the
// level, and a trap is a crash in the middle of a tick. Neither is silent -- a saturated value
// stays pinned at the limit, which is visible, while a wrapped one looks like plausible data.
const fix32 top = fix32::from_raw(INT32_MAX);
show("saturated add", top + fix32::from_int(1));
show("division by zero", fix32::from_int(1) / fix32::from_int(0));
```
<!-- /snippet -->

У насыщения есть цена: значение, упёршееся в предел, неверно — просто неверно ЗАМЕТНО. Подсистемы,
которые могут задать себе собственные потолки, задают их — и клампят сильно ниже насыщения:
персонаж с ускорением за потолком не «очень резвый», он приезжает в скорость, посчитанную из
насыщенного произведения.

Программа печатает:

<!-- snippet: docs/examples/fixed_point.out -->
```text
3 * 0.5                      raw 0x00018000
1 / 3                        raw 0x00005555
0.1 + 0.2                    raw 0x00004ccd
saturated add                raw 0x7fffffff
division by zero             raw 0x7fffffff
```
<!-- /snippet -->

`1 / 3` — это `0x5555`, а не треть: представление конечно, и деление усекает. Здесь фиксированная
точка перестаёт быть «плавающей на целых»: вы выбираете, какие именно разряды существуют, и любая
машина выбирает те же самые.

## На чём ещё стоит обещание

Одной арифметики мало. Порядок исполнения обязан ВЫВОДИТЬСЯ, а не запоминаться — этим занято
[расписание](tick-and-schedule.md), — а всё, что симуляция читает, обязано быть частью записанного
состояния. Настенное время, адрес выделенной памяти, порядок обхода хеш-контейнера с ключом-указателем:
каждое из этого — машинно-зависимый вход, и ни одно не выглядит таковым в точке вызова.
