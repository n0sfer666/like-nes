# Минимальный iOS — один источник: флаги clang, MinimumOSVersion в platform/ios/Info.plist.in и цель
# cargo для wgpu-native читают эту переменную. Без неё clang берёт версию SDK (27.0), а plist обещал
# 14.0, и приложение, заявленное для старых iPhone, на них не запускалось. Включается корнем ДО
# project(): от неё зависят проверки тулчейна. 17.0 — рекомендованная цель Xcode 27; прогоняется
# только iOS 27. Согласие трёх мест проверяет platform/mobile/ios_min_os.sh.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0 CACHE STRING "Minimum iOS version of the iOS shell")
endif()
