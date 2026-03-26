# MeshTRX iOS — Лог разработки

---

## 2026-03-24 — Начало проекта

### Решения
- iOS-приложение будет портом Android версии 4.2.0 (полный паритет функций)
- Стек: Swift/SwiftUI, CoreBluetooth, AVAudioEngine, MapKit
- ~~Среда разработки: Docker-OSX на Linux~~ — kernel panic на AMD Ryzen 7 5800H (Sonoma, Ventura, Monterey — все падают)
- **Среда разработки: Mac Mini 2012 (16 ГБ RAM, SSD)** — нативный Mac
- macOS: **Monterey 12** через OpenCore Legacy Patcher (Ventura не встала ранее)
- Xcode: **14.2** (Swift 5.7, iOS 16 SDK, SwiftUI 4.0)
- Тестовое устройство: iPhone (есть) + Heltec V3 (есть)
- Бюджет: $0

### Создано
- `ios/IMPLEMENTATION_PLAN.md` — план реализации из 17 этапов (~35-50 дней)
- `ios/DEV_LOG.md` — этот лог

### Следующий шаг
- Поднять Docker-OSX для среды разработки

---

## 2026-03-24 — Настройка Docker-OSX

### Предварительные требования
- Linux с поддержкой KVM (Intel VT-x / AMD-V)
- Docker установлен
- X11 для GUI (дисплей)

### Процедура установки

#### Шаг 1 — Проверка KVM
```bash
# Проверить поддержку аппаратной виртуализации
egrep -c '(vmx|svm)' /proc/cpuinfo
# Должно быть > 0

# Проверить KVM модуль
ls /dev/kvm
# Должен существовать

# Если нет — загрузить модуль
sudo modprobe kvm
sudo modprobe kvm_intel  # или kvm_amd
```

#### Шаг 2 — Права на KVM
```bash
# Добавить пользователя в группу kvm
sudo usermod -aG kvm $USER
# Перелогиниться или:
newgrp kvm
```

#### Шаг 3 — Скачивание образа
```bash
# Тег :sonoma больше не существует, используем :latest
# macOS выбирается через SHORTNAME при запуске
docker pull sickcodes/docker-osx:latest
```

#### Шаг 4 — Запуск Docker-OSX (macOS Sonoma)
```bash
docker run -it \
  --device /dev/kvm \
  -p 50922:10022 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -e "DISPLAY=${DISPLAY}" \
  -e "SHORTNAME=sonoma" \
  -e "RAM=12" \
  -e "CPUS=8" \
  -e "DISK_SIZE=80G" \
  sickcodes/docker-osx:latest
```

Параметры:
- `SHORTNAME=sonoma` — macOS 14 (минимум для Xcode 16)
- `RAM=12` — 12 ГБ RAM для виртуалки (Xcode прожорлив)
- `CPUS=8` — 8 ядер из 16 доступных
- `DISK_SIZE=80G` — 80 ГБ диск (Xcode ~30 ГБ + проект + кэш)

> Доступные SHORTNAME: high-sierra, mojave, catalina, big-sur, monterey, ventura, sonoma, sequoia, tahoe

#### Шаг 5 — Установка macOS
- Пройти macOS Setup Assistant в GUI-окне
- Создать пользователя
- Дождаться загрузки рабочего стола

#### Шаг 6 — Установка Xcode
- Открыть App Store → войти Apple ID → скачать Xcode
- Или скачать .xip с developer.apple.com
- После установки: `xcode-select --install` (Command Line Tools)

#### Шаг 7 — Сохранение состояния (важно!)
```bash
# Найти ID контейнера
docker ps

# Сохранить контейнер как образ (чтобы не переустанавливать)
docker commit <container_id> meshtrx-macos

# Следующий запуск из сохранённого образа:
docker run -it \
  --device /dev/kvm \
  -p 50922:10022 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -e "DISPLAY=${DISPLAY}" \
  meshtrx-macos
```

#### Шаг 8 — USB Passthrough для iPhone
```bash
# Найти iPhone USB ID
lsusb | grep -i apple

# Запуск с проброcом USB (пример для Apple device 05ac:12a8)
docker run -it \
  --device /dev/kvm \
  --device /dev/bus/usb \
  --privileged \
  -p 50922:10022 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -e "DISPLAY=${DISPLAY}" \
  meshtrx-macos
```

#### Шаг 9 — Проброс папки проекта
```bash
# Монтируем папку ios/ внутрь виртуалки через SSH
# Сначала из контейнера:
ssh -p 50922 user@localhost

# Или через volume (доступ из macOS через virtio):
docker run -it \
  --device /dev/kvm \
  -p 50922:10022 \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v /home/datasub/@PROJECTS/meshtrx/ios:/mnt/shared \
  -e "DISPLAY=${DISPLAY}" \
  meshtrx-macos
```

### Проверка системы (результаты)
| Параметр | Значение | Статус |
|----------|----------|--------|
| KVM | 16 vCPU, vmx | OK |
| /dev/kvm | crw-rw---- root:kvm | OK |
| Docker | 29.3.0 | OK |
| RAM | 38 ГБ (21 ГБ свободно) | OK |
| CPU | 16 ядер | OK |
| Диск | 100 ГБ свободно | OK |
| Группа kvm | добавлен через usermod + newgrp | OK |

### Docker-OSX — не сработал
- AMD Ryzen 7 5800H → kernel panic на всех версиях macOS (Sonoma, Ventura, Monterey)
- CPU=Penryn + vendor=GenuineIntel не помогло
- Решение: использовать Mac Mini 2012 (физический)

---

## 2026-03-24 — Подготовка загрузочной USB для Mac Mini 2012

### Конфигурация Mac Mini 2012
- RAM: 16 ГБ
- Диск: SSD
- Текущая ОС: Windows 10
- Цель: macOS Monterey 12 через OCLP

### Подготовка USB-флешки
- Устройство: /dev/sda, 14.4 ГБ
- Отформатирована: GPT + FAT32, метка MONTEREY
- gibMacOS клонирован в /tmp/gibMacOS
- Скачивание macOS Monterey запущено

### Статус
- [x] USB отформатирована (exFAT, метка MONTEREY)
- [x] gibMacOS скачан
- [x] macOS Monterey 12.7.6 скачана (InstallAssistant.pkg, 12 ГБ)
- [x] OpenCore Legacy Patcher 2.4.1 скачан (OpenCore-Patcher.pkg, 702 МБ)
- [x] Оба файла скопированы на USB-флешку
- [x] macOS Catalina установлена на Mac Mini через Internet Recovery (Win+Alt+R)
- [x] OCLP 2.4.1 установлен, OpenCore записан на внутренний диск
- [x] macOS Monterey 12.7.6 установлена через OCLP "Create macOS Installer" → "Use existing macOS Installer"
- [ ] macOS Tahoe (16) — обновление запущено из Software Update (2026-03-25)
- [ ] Post-Install Root Patch (OCLP) — после завершения обновления
- [ ] Xcode установлен

### Заметки по процессу установки
- Docker-OSX на AMD Ryzen 7 5800H — kernel panic на всех версиях, не вариант
- USB флешка с dd BaseSystem.img — Mac видит, но Monterey recovery даёт знак "запрещено" (несовместима)
- Catalina recovery через macrecovery (board ID Mac-7BA5B2DFE22DDD8C, -os default) — работает
- Internet Recovery: **Win+Alt+R** при включении (PC-клавиатура)
- Startup Manager: **Alt** при включении
- На USB-флешке (exFAT, /dev/sda) остались InstallAssistant.pkg (Monterey 12.7.6) и OpenCore-Patcher.pkg (OCLP 2.4.1) — пересоздать если нужно, файлы в /tmp могут быть удалены

### Следующие шаги после загрузки Catalina
1. Скачать OCLP 2.4.1 (https://github.com/dortania/OpenCore-Legacy-Patcher/releases/download/2.4.1/OpenCore-Patcher.pkg)
2. Установить OCLP → Build and Install OpenCore на внутренний диск
3. Перезагрузить
4. Запустить InstallAssistant.pkg (Monterey 12.7.6) — скачать с Apple или перенести с флешки
5. Установить Monterey
6. После Monterey → OCLP → Post-Install Root Patch
7. Установить Xcode 14.2 из App Store или developer.apple.com

---

## Инструкция: установка macOS Monterey на Mac Mini 2012

### Шаг 1 — Установить базовую macOS (Catalina) через Internet Recovery
1. Вставить флешку в Mac Mini (пока не нужна, но пусть будет)
2. Включить Mac Mini, сразу зажать **Cmd + Option + R**
3. Держать до появления глобуса (Internet Recovery)
4. Подключиться к Wi-Fi если попросит
5. Дождаться загрузки Recovery (~5-10 мин)
6. **Disk Utility** → выбрать внутренний SSD → **Erase** (APFS, GUID)
7. Закрыть Disk Utility → **Reinstall macOS** → выбрать SSD
8. Ждать установки (~30-40 мин)
9. Пройти Setup Assistant (создать пользователя)

### Шаг 2 — Запустить InstallAssistant.pkg
1. После загрузки Catalina — открыть флешку MONTEREY в Finder
2. Двойной клик по **InstallAssistant.pkg**
3. Установится приложение "Install macOS Monterey" в /Applications
4. **НЕ запускать его пока!**

### Шаг 3 — Установить OpenCore Legacy Patcher
1. Двойной клик по **OpenCore-Patcher.pkg** на флешке
2. Установить OCLP
3. Запустить **OpenCore-Patcher** из Applications
4. Нажать **Build and Install OpenCore**
5. Выбрать **внутренний диск** (не флешку!) для установки OpenCore
6. После завершения — **перезагрузить Mac**

### Шаг 4 — Загрузиться через OpenCore и установить Monterey
1. При загрузке появится меню OpenCore → выбрать основной диск
2. Загрузиться в Catalina
3. Запустить **Install macOS Monterey** из /Applications
4. Следовать инструкциям установщика
5. Mac перезагрузится несколько раз (каждый раз выбирать "macOS Installer" в меню OpenCore)
6. После завершения — Setup Assistant → войти

### Шаг 5 — Применить патчи OCLP
1. После загрузки Monterey — запустить **OpenCore-Patcher**
2. Нажать **Post-Install Root Patch**
3. Применить патчи (графика, Wi-Fi)
4. Перезагрузить

### Шаг 6 — Установить Xcode
1. Открыть App Store → найти Xcode
2. Или скачать Xcode 14.2 с developer.apple.com/download/more/
3. После установки: открыть Terminal → `xcode-select --install`
4. Готово!
