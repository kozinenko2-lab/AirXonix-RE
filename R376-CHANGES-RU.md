# AirXonix Wi-Fi Co-op r376 — итерация TCP-устойчивости

База: r375 из Library. Режим только для PortMaster/H700; Android и обычный одиночный AirXonix не затрагивались.

## Что изменено

1. В `src/net/lan_transport.hpp/.cpp` добавлен тайм-аут отсутствия входящих байтов 3500 мс по `std::chrono::steady_clock`. Это позволяет обнаружить пропавший Wi-Fi даже когда TCP формально ESTABLISHED.
2. На хосте в `src/main.cpp` отсутствие свежих кнопок гостя более 500 мс теперь замораживает всю `Game::updateCoop()` — не только его управление. Поэтому общие время, жизни и враги не продвигаются, пока сеть недоступна.
3. На сетевых переходах READY/LOST сбрасываются `guestHeld` и `lastGuestInputMs`, чтобы после повторного соединения красный Xonix не продолжал движение по старым командам.
4. После сетевого обрыва хост сохраняет сокет прослушивания. При повторном `accept` состояние протокола и устаревшая ошибка очищаются.
5. Добавлен автономный TCP-тест `tests/test_lan_timeout_r376.cpp` (не требует SDL/GLES), зарегистрированный для CTest при `AIRXONIX_BUILD_TESTS=ON`.

## Проверки на хосте Linux

- `g++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread -Isrc src/net/lan_transport.cpp tests/test_lan_timeout_r376.cpp -o test_lan_timeout_r376 && ./test_lan_timeout_r376` — PASS.
- Тот же тест с `-fsanitize=address,undefined` — PASS.
- `cmake -S . -B build -DAIRXONIX_BUILD_RUNTIME=OFF -DAIRXONIX_BUILD_TESTS=OFF` — PASS.
- `cmake --build build --target airxonix_gamecore` — PASS.

**Не проверено:** полная сборка `airxonix_mp` под Cortex-A53; запуск и лаги на двух физических RG40XX H. Не используйте двоичный пакет r375 для проверки исправлений r376.
