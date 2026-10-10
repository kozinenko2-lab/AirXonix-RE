# AirXonix Android r373 — публичный исходный код

Этот проект содержит **исходный код** Android ARM64-версии, включая Gradle, SDL2-интеграцию, сенсорное управление, Java-обвязку и нативный рендерер GLES2. Для полного описания сборки используйте [README.md](README.md).

## Сборка без оригинальных ресурсов игры

Нужны JDK 17, Android SDK Platform 35, NDK 27.2.12479018, CMake 3.22.1, Gradle 8.7 и доступ к исходникам SDL2 2.30.12.

```bash
python3 -m pip install numpy pillow
python3 native/tools/generate_cleanroom_resources.py generated/cleanroom
mkdir -p app/src/main/assets
cp generated/AirXonix-cleanroom.zip app/src/main/assets/
bash scripts/prepare_sdl2.sh
gradle --no-daemon :app:assembleDebug
```

Скомпилированный `app-debug.apk` находится в `app/build/outputs/apk/debug/`. Присутствует GitHub Actions workflow `.github/workflows/android-apk.yml`, который выполняет такую же сборку на GitHub.

**Примечание:** исходный код r373 из публичного архива **не содержит** оригинальных EXE, музыки, коммерческих текстур и ключей подписи APK. Личная версия с оригинальными ресурсами в GitHub не публикуется.

## Управление

Виртуальный стик (четыре направления), сенсорные A/B, пауза, мультитач, USB/Bluetooth-геймпад и экранная клавиатура при вводе имени рекорда.

**Тестирование:** рабочий APK r373 существовал отдельно, но актуальная сборка этой публичной ветки через GitHub Actions ещё должна пройти. Наличие исходного кода не подтверждает прохождение игры на всех Android-устройствах.

Для мультиплеера H700 см. ветку [portmaster](https://github.com/kozinenko2-lab/AirXonix-RE/tree/portmaster).
