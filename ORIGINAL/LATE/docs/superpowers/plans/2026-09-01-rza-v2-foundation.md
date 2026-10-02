# RZA v2 Foundation — план реализации

> **Для агентных исполнителей:** ОБЯЗАТЕЛЬНЫЙ ПОДНАВЫК: использовать `superpowers:subagent-driven-development` (рекомендуется) или `superpowers:executing-plans`, выполняя этот план по задачам с чекбоксами.

**Цель:** Создать воспроизводимую CMake/CTest-основу, отдельно собирающую legacy и минимальный v2 CLI с первым автоматическим тестом.

**Архитектура:** Корневой CMake подключает независимые `rza_legacy` и `rza_v2_core`/`rza`. Тесты не зависят от реальных данных или сети; общий header предоставляет минимальные assertions и deterministic runner.

**Стек:** C++17, CMake 4.4+, CTest, MinGW-w64 GCC 16.1, PowerShell, Git.

**Спецификация:** `docs/superpowers/specs/2026-09-01-rza-v2-design.md`

## Общие ограничения

- Исходники legacy не изменять.
- Сборка выполняется в `out/build/<preset>`, не в `src` и не в старом `build`.
- Для v2 warnings являются ошибками; legacy сохраняет текущие предупреждения без `-Werror`.
- Foundation не реализует торговую логику и не добавляет внешние зависимости.
- Все пути внутри CMake относительные; сборка не запускает analyzer автоматически.

---

### Задача 1: Корневая CMake-сборка и presets

**Файлы:**
- Создать: `CMakeLists.txt`, `CMakePresets.json`, `cmake/RzaWarnings.cmake`, `src/v2/CMakeLists.txt`
- Изменить: `.gitignore`

**Интерфейсы:**
- Создаёт targets `rza_v2_core`, `rza`, опциональный `rza_legacy`.
- Опции: `RZA_BUILD_LEGACY=ON`, `RZA_BUILD_TESTS=ON`, `RZA_WARNINGS_AS_ERRORS=ON` только для v2.

- [ ] Добавить failing configure-check: выполнить `cmake --preset windows-debug` и зафиксировать ожидаемый FAIL из-за отсутствующего `CMakeLists.txt`.
- [ ] Создать корневой project `RectangleZoneAnalyzer VERSION 2.0.0 LANGUAGES CXX`, потребовать C++17 без extensions, включить CTest и подкаталоги.
- [ ] В `RzaWarnings.cmake` определить `rza_enable_warnings(target, errors)` с GCC-флагами `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`; `-Werror` добавлять только при `errors`.
- [ ] В preset использовать generator `MinGW Makefiles`, binaryDir `${sourceDir}/out/build/${presetName}`, Debug/Release и соответствующий путь к `D:/AHexaTrader/COMPILER/bin/g++.exe` через cache variable `CMAKE_CXX_COMPILER`.
- [ ] Добавить `/out/`, `/Testing/` и CMake user presets в `.gitignore`.
- [ ] Запустить `cmake --preset windows-debug`; ожидать exit 0 и generated files только под `out/build/windows-debug`.
- [ ] Commit: `build: add reproducible CMake foundation`.

### Задача 2: Минимальный v2 core и CLI

**Файлы:**
- Создать: `src/v2/core/build_info.h`, `src/v2/core/build_info.cpp`, `src/v2/cli/main.cpp`
- Изменить: `src/v2/CMakeLists.txt`

**Интерфейсы:**
- `rza::v2::BuildInfo { int major, minor, patch; std::string_view name; }`
- `BuildInfo build_info() noexcept` возвращает `{2,0,0,"RectangleZoneAnalyzer"}`.
- `rza --version` печатает `RectangleZoneAnalyzer 2.0.0` и возвращает 0; неизвестный аргумент возвращает 2.

- [ ] Создать compile-failing consumer, включающий `v2/core/build_info.h`, до появления header; запустить build и подтвердить FAIL `No such file`.
- [ ] Реализовать `BuildInfo` и CLI ровно с двумя режимами: без аргументов печатает краткую справку, `--version` печатает версию.

```cpp
namespace rza::v2 {
struct BuildInfo { int major; int minor; int patch; std::string_view name; };
BuildInfo build_info() noexcept { return {2, 0, 0, "RectangleZoneAnalyzer"}; }
}
```
- [ ] Собрать: `cmake --build --preset windows-debug --target rza`; ожидать exit 0 без v2 warnings.
- [ ] Выполнить `out/build/windows-debug/src/v2/rza.exe --version`; ожидать точную строку и exit 0.
- [ ] Commit: `feat(core): add v2 build identity and CLI`.

### Задача 3: Автономный тестовый runner и первые тесты

**Файлы:**
- Создать: `tests/CMakeLists.txt`, `tests/support/test_runner.h`, `tests/unit/core/test_build_info.cpp`, `tests/acceptance/test_cli_version.ps1`
- Изменить: корневой `CMakeLists.txt`

**Интерфейсы:**
- `RZA_TEST(name)` регистрирует функцию; `RZA_REQUIRE(expr)` печатает файл/строку и завершает текущий case ошибкой.
- `rza_v2_tests` регистрируется в CTest как `unit.v2`.
- PowerShell acceptance test принимает обязательный `-Executable`, проверяет output и `$LASTEXITCODE`.

- [ ] Написать тест, ожидающий `build_info().major == 2`, `minor == 0`, `patch == 0`, `name == "RectangleZoneAnalyzer"`; до подключения test target `ctest` должен сообщить отсутствие тестов.

```cpp
RZA_TEST(build_info_has_approved_v2_identity) {
    const auto info = rza::v2::build_info();
    RZA_REQUIRE(info.major == 2);
    RZA_REQUIRE(info.minor == 0);
    RZA_REQUIRE(info.patch == 0);
    RZA_REQUIRE(info.name == "RectangleZoneAnalyzer");
}
```
- [ ] Реализовать deterministic runner: stable registration order, счётчики passed/failed, exit 1 при любом сбое, без catch-all подавления.
- [ ] Зарегистрировать unit и CLI acceptance tests через `add_test`.
- [ ] Запустить `cmake --build --preset windows-debug` и `ctest --preset windows-debug --output-on-failure`; ожидать 2/2 PASS.
- [ ] Повторить Release configure/build/test; ожидать 2/2 PASS.
- [ ] Commit: `test: add offline v2 test foundation`.

### Задача 4: Отдельная legacy-цель и clean-build инструкция

**Файлы:**
- Создать: `cmake/LegacyTarget.cmake`, `README.md`
- Изменить: корневой `CMakeLists.txt`

**Интерфейсы:**
- `rza_legacy` включает текущие `src/*.cpp` рекурсивно, исключая `src/v2`, использует include `${PROJECT_SOURCE_DIR}/src`, C++17 и warnings без `-Werror`.
- README описывает prerequisites, configure/build/test, отдельный legacy build и запрет официальных результатов legacy.

- [ ] До подключения target выполнить build `--target rza_legacy`; ожидать FAIL `No rule to make target`.
- [ ] Реализовать legacy target без правок legacy source и без автоматического запуска.
- [ ] Собрать `rza_legacy`; ожидать exit 0, разрешая только уже известные warnings unused `argc/argv` и unused `equity`.
- [ ] Выполнить полный Debug и Release test gate, затем `git diff --check` и secret scan подготовленных файлов.
- [ ] Добавить README с точными командами и статусом legacy/v2.
- [ ] Commit: `docs: document reproducible v2 and legacy builds`.

### Задача 5: Foundation verification gate

**Файлы:** только проверка; изменения допустимы лишь для устранения обнаруженного дефекта с отдельным RED/GREEN циклом.

- [ ] Из чистого нового `out/build/verification-debug` выполнить configure, full build и CTest.
- [ ] Из `out/build/verification-release` повторить configure, full build и CTest.
- [ ] Проверить точный `rza --version`, неизвестный аргумент и exit codes.
- [ ] Проверить `git status --short`, `git diff --check`, отсутствие tracked `out/`, `.exe`, `.o`, cache, log и credential helper.
- [ ] Зафиксировать фактические версии CMake 4.4.0 и GCC 16.1.0 в README; Ninja не заявлять, поскольку он отсутствует.
- [ ] Если исправления не потребовались, commit не создавать. Сохранить команды и результаты проверки в итоговом handoff.
