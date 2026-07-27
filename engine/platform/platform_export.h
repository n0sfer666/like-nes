#pragma once

// Экспорт символа из загружаемого модуля (.so/.dylib/.dll).
//
// ELF и Mach-O отдают глобальные символы наружу по умолчанию, поэтому на POSIX пометка была
// не нужна. У PE ровно наоборот: без __declspec(dllexport) символа в таблице экспорта нет, и
// GetProcAddress вернёт nullptr — сбой тихий, в рантайме, а не на линковке. Пометка обязана
// стоять на КАЖДОЙ функции границы плагина.
//
// Атрибут visibility на POSIX-ветке — не декорация: он держит границу открытой, если сборку
// когда-нибудь переведут на -fvisibility=hidden.
#if defined(_WIN32)
#define PLATFORM_EXPORT __declspec(dllexport)
#else
#define PLATFORM_EXPORT __attribute__((visibility("default")))
#endif
