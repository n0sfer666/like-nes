#pragma once

namespace rumble {

// Строка доказывает, что нативный ввод этой ОС слинкован из префикса: без вызова линкер выбросил бы
// архив, и гейт sdk-game судил бы игру без него.
void report_pad_backend();

} // namespace rumble
