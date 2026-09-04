# Штамп версии — ОДИН на дерево. Раньше версия, короткий хеш и целевая тройка считались в
# packaging.cmake игры-образца, и второй потребитель (пакет движка, спека #20) обязан был бы
# повторить тот же код: два места, считающие версию по-разному, — это релиз, который называет себя
# двумя именами. Файл включается дважды и не портится от этого: guard ниже.
if(DEFINED LIKE_NES_GIT_HASH)
  return()
endif()

execute_process(COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
  OUTPUT_VARIABLE LIKE_NES_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT LIKE_NES_GIT_HASH)
  set(LIKE_NES_GIT_HASH "unknown")
endif()

# Версия приходит из тега (`release.yml` и scripts/release.sh: -DGAME_VERSION=vX.Y.Z). Дефолт
# помечен `-dev` намеренно: пакет, собранный без тега, обязан называть себя не релизом.
if(NOT GAME_VERSION)
  set(GAME_VERSION "0.1.0-dev")
endif()
set(LIKE_NES_VERSION "${GAME_VERSION}")
# Тройку разрешено НАЗВАТЬ снаружи (-DLIKE_NES_TARGET_TRIPLE=macos-arm64): её же несёт имя
# релизного пакета, и пока их считали двое, штамп говорил `Darwin-arm64` внутри архива с именем
# `macos-arm64` — то есть архив, названный одной платформой, мог нести штамп другой, и ни одно
# утверждение гейта этого не видело. Источник теперь один: кто назвал имя пакета, тот назвал и
# штамп. Не назвал никто (обычная сборка) — прежняя пара от CMake.
if(NOT LIKE_NES_TARGET_TRIPLE)
  set(LIKE_NES_TARGET_TRIPLE "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()
