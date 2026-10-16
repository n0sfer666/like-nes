# Мобильная половина корня: оболочка, приложения на ней и мобильный префикс SDK (спека #26, В1).

# Мобильная сборка строит только замыкание оболочки: тесты и демо движка под iOS/Android не
# запускаются, а собранные «на всякий случай» они требовали бы от шва того, чего на телефоне нет.
get_property(dirs DIRECTORY PROPERTY SUBDIRECTORIES)
foreach(dir IN LISTS dirs)
  set_property(DIRECTORY ${dir} PROPERTY EXCLUDE_FROM_ALL TRUE)
endforeach()

# Оболочки дерева собирают приложения той же функцией, что игра против SDK: здесь её переменные
# указывают в дерево, в префиксе — в share/like-nes/.
include(${CMAKE_SOURCE_DIR}/cmake/licenses.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/like_nes_mobile_app.cmake)
set(LIKE_NES_MOBILE_LIB like_nes_mobile)
set(LIKE_NES_MOBILE_SHARE ${CMAKE_SOURCE_DIR}/platform)
set(LIKE_NES_MOBILE_LICENSES ${LICENSE_FILES})

string(TOLOWER ${CMAKE_SYSTEM_NAME} shell)
add_subdirectory(${CMAKE_SOURCE_DIR}/platform/${shell})
include(${CMAKE_SOURCE_DIR}/cmake/install_sdk.cmake)
