# В6: private RT controller перед MusicStream

MusicRuntime — законченная private ответственность, ещё без Mixer binding, SDK export и
публичного MusicStream. Два кольца/decoder slots, отдельные SPSC game→RT и RT→worker;
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
Будущий Mixer oracle с delay47/correct negative Q16 floor — c3667d5c010ee2c0; подключение Mixer
пока не реализовано. Тайминг oracle задаёт manual pump, произвольный scheduling не пинится.

Три теста используют настоящий MusicWorker/decoder/rings: loops/intro, ready delay, replacement
cached primer, repeats/reverse/shortfall returns, stop/newtag/fullqueue/latest retry, actual
openable corrupt Ogg/read failure, failed held recovery, exact underruns, closed cache и
10000 deterministic churn steps с failures/UINT32_MAX fades и последующим восстановлением.
Fixtures холодно на heap из-за Windows stack1MiB. Ручной worker вызывается вне allocator hot
region; plain/aligned controls и все RT transitions дают0heap. Полный concurrent RT mutex gate
и lifetime/TSan интеграция остаются следующими пакетами, В6 не объявлена завершённой.

Release/Debug/ASan+UBSan PASS; старые mono/stereo goldens, RT/gain/limiter PASS.14 actual Cpp
mutants в копиях настоящего SANarchive отвергнуты: time/third/reverse, failure/hold/repeat,
pending/gen/cache/closed и heap; исходники не менялись, baseline PASS. Диагностика и oracle
сохранены в ignored build-sdk-work/v6-vorbis/runtime-mutants и runtime-oracle.*.
SDK Release/Debug+iOSsim/Androidx86_64 PASS. Новый mobile runtime здесь не запускался:
ограничения предыдущего этапа сохраняются. Финальные types/lint и exact staged review — перед коммитом.
