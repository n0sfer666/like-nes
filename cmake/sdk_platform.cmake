# Платформа сборки одним словом (спека #26, В1): `desktop`, `ios-sim`, `ios-device`,
# `android-<abi>`. Её записывает в Config дерево, ставящее префикс, и с ней Config сверяет сборку
# игры: префикс симулятора в сборке под телефон слинковался бы с объектами чужой цели, а не отказал.
function(like_nes_sdk_platform out)
  if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(CMAKE_OSX_SYSROOT MATCHES "iPhoneSimulator|iphonesimulator")
      set(${out} ios-sim PARENT_SCOPE)
    else()
      set(${out} ios-device PARENT_SCOPE)
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(${out} "android-${ANDROID_ABI}" PARENT_SCOPE)
  elseif(CMAKE_SYSTEM_NAME MATCHES "^(Windows|Darwin|Linux)$")
    set(${out} desktop PARENT_SCOPE)
  else()
    # tvOS, Emscripten и прочие словом `desktop` приняли бы десктопный префикс и упали бы на
    # линковке; своё слово не совпадёт ни с одним префиксом, и откажет find_package.
    set(${out} "unsupported-${CMAKE_SYSTEM_NAME}" PARENT_SCOPE)
  endif()
endfunction()

# Тройка ABI Android — она же каталог sysroot NDK, префикс его clang и цель Rust: одна на ABI.
function(like_nes_android_triple abi out)
  if(abi STREQUAL "arm64-v8a")
    set(${out} aarch64-linux-android PARENT_SCOPE)
  elseif(abi STREQUAL "x86_64")
    set(${out} x86_64-linux-android PARENT_SCOPE)
  else()
    message(FATAL_ERROR "like-nes: unsupported ANDROID_ABI '${abi}'")
  endif()
endfunction()
