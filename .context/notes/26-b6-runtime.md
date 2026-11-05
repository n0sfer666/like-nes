# В6: RT controller, Mixer и MusicStream

MusicRuntime — private controller, подключённый к Mixer публичным MusicStream. Два кольца/decoder slots, отдельные SPSC game→RT и RT→worker;
next выдаёт один weighted PCM sample в Q16 и число недобранных активных slot-frames.
Каталог1..4096 задаётся холодно, lookup/asset pointers в RT нет. Caller сохраняет wish при full.

Generation меняется до load; stop получает новый tag. Full worker queue сохраняет последний
pending load; cursor reset отсутствует. Priming отбрасывает stale PCM, кэширует максимум первый
matching frame и ждёт ready12000. Замена incoming сбрасывает cache; failed tag не воспроизводится.
Closed подавляет даже готовый cached primer без audio ack. Runtime/transport живут до join сторон.

Gain mode Held/Rising/Falling отделяет повтор incoming от возврата outgoing: повтор не продлевает
рампу. При ожидании incoming громкость outgoing удерживается. Reverse берёт текущие gains и
сохраняет позиции; fade0 мгновенный. Third wish: старый outgoing затихает480samples, incoming
держит gain, последующие wishes coalesce без продления480. Возврат к held завершает480 и затем
поднимает его gain; возврат к ещё звучащему outgoing разворачивает существующие рампы.
Отказ held decoder во время480 освобождает слот для pending cue, не оставляет его ждать навсегда.
Нулевой Held при очень длинном fade сохраняет decoder/cursor: освобождается только завершённый
Falling. Regression наблюдает один tag во время480 и точный PCM после65536samples возобновлённой рампы.

Независимый Python Fraction/LE oracle2300frames: Runtime Q16 hash3023e102d993ecb4. Второй Sol High
самостоятельно воспроизвёл hash и boundary gains/PCM, не читая основной oracle или Runtime.
Mixer oracle с delay47/correct negative Q16 floor — c3667d5c010ee2c0; при BusMusic/master .75
scaled hash94b29cb8987f09c3. Оба backend и blocks1/17/47/480/800 проверены. Тайминг oracle задаёт manual pump, произвольный scheduling не пинится.

Три теста используют настоящий MusicWorker/decoder/rings: loops/intro, ready delay, replacement
cached primer, repeats/reverse/shortfall returns, stop/newtag/fullqueue/latest retry, actual
openable corrupt Ogg/read failure, failed held recovery, exact underruns, closed cache и
10000 deterministic churn steps с failures/UINT32_MAX fades и последующим восстановлением.
Fixtures холодно на heap из-за Windows stack1MiB. Ручной worker вызывается вне allocator hot
region; plain/aligned controls и все RT transitions дают0heap. Полный concurrent RT mutex gate
остаётся следующим пакетом, В6 не объявлена завершённой.

Release/Debug/ASan+UBSan PASS; старые mono/stereo goldens, RT/gain/limiter PASS.14 actual Cpp
mutants в копиях настоящего SANarchive отвергнуты: time/third/reverse, failure/hold/repeat,
pending/gen/cache/closed и heap; исходники не менялись, baseline PASS. Диагностика и oracle
сохранены в ignored build-sdk-work/v6-vorbis/runtime-mutants и runtime-oracle.*.
SDK Release/Debug+iOSsim/Androidx86_64 PASS. Новый mobile runtime здесь не запускался:
ограничения предыдущего этапа сохраняются. Финальные types/lint и exact staged review — перед коммитом.

## Публичный Stream и lifetime

Одна успешная startup-привязка на Mixer/Runtime. Mixer держит shared pin, callback читает raw
pointer без refcount/deletion. MusicStream владеет worker/thread, close+join не ждёт audio ack.
Mixer уничтожается после внешнего callback stop/join; handle/stats могут пережить Mixer.
Metadata копируются при init; borrowed bytes неизменны до Stream destructor/join.
MusicStats хранятся в Runtime и безопасны для параллельного чтения живого handle.

MusicControl — единственный production путь выбора: exact N forward ticks, unknown IDs не
стирают желание, IDs0/MAX допустимы, queue-full сохраняет последнее устойчивое желание.
reset_intent после Session reset/restart очищает observer/latest/неопубликованный pending,
сохраняет N; принятые wishes и DSP продолжаются. Без нового want тики не учитываются.

Dedicated music не занимает ни один из32 SFX voices/16 resident sources. Обе реализации Mixer
добавляют weighted Q16 до BusMusic/duck/master/limiter; Fix не округляет PCM преждевременно.
Детерминированные Control/Runtime тесты проверяют точные границы, публичный actual-Ogg тест —
реальную worker интеграцию и новую tick0 эпоху. Lifetime тесты проверяют оба порядка уничтожения,
освобождение metadata/source, paused callback с full wishes/rings. Concurrent тест обязан
услышать PCM и увидеть два decoder до churn; TSan проверяет callback/control/worker/observer.

Release/Debug/ASan+UBSan/TSan, types/lint и установленный SDK Release/Debug+iOSsim/Androidx86_64
PASS. В CI исправлены ручные Mixer source closures; их SAN RT/golden и TSan determinism команды
локально PASS.20 actual source mutants в SAN archive отвергнуты; compile/link успешны, baseline
PASS. Диагностика сохранена в ignored build-sdk-work/v6-vorbis/stream-mutants и stream-manual.
Новый mobile runtime не проверен: adb devices пуст, прежние ограничения simulator остаются.

## Полный RT gate

Тест audio_music_rt_test использует optional TLS общего allocation probe; прежние defaults и
операторы неизменны. Отдельный platform test shim: Apple dylib __DATA,__interpose, Linux --wrap
executable/static references (DSO internals не охвачены), Windows явно allocation-only.
Control TU живёт в executable: direct pthread и std::mutex обязаны дать2 перехвата на Unix.
Plain/aligned allocations обязаны считаться, а выделения/locks cold thread при гарантированном
пересечении hot region не должны приписываться callback. Общий g_escape helper вызывается
только последовательно. Actual Ogg/PCM worker, оба backend, минимум два decoder до churn;
callback измеряется также во время close/join и после source free. Пробы не линкуются в продукт.
Новый RT gate запускается R/D отдельно от sanitizer/interposer, functional ASan/UBSan и TSan
проверки остаются обязательными; Windows/Linux результаты требует соответствующая CI.

RT gate R/D и default allocation-probe regression gates PASS. Четыре actual callback source
mutants (new/aligned new/stdmutex/pthread) отвергнуты, compile/link успешны, baseline PASS;
manual timeline дополнительно измеряет third/reverse/closed. Functional ASan/UBSan и TSan PASS.
GCC16 собрал production audio и шесть functional tests, оба Mixer golden прежние. Артефакты
ignored rt-mutants и gcc-music сохранены для независимого review и диагностики.
