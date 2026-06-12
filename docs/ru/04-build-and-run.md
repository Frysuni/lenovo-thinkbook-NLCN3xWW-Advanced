# 4. Сборка и запуск

## Два файла на флешке

1. **`BiosAdvancedPatch.efi`** — рантайм-патчер этого проекта
   (`patcher/prebuilt/BiosAdvancedPatch.efi`, либо соберите — ниже).
2. **`SetupUtilityApp.efi`** — *собственная* утилита настройки вашего ноутбука,
   извлечённая из его прошивки. Проект её **не** поставляет (это копирайтный
   бинарь Lenovo). Извлеките её из своего образа BIOS через UEFITool: это
   PE32-секция DXE-приложения `SetupUtilityApp`. Сохраните на флешку под любым
   именем, например `SetupUtilityApp.efi`.

Оба файла кладутся на **FAT32** USB-флешку (в корень или любую папку).

## Готовый бинарь

```
patcher/prebuilt/BiosAdvancedPatch.efi
SHA256: eb0c1d6e4ded03db4a86bcd26f1cddd5be274ccab31331d1473211d7574346e6
```

Это воспроизводимая сборка (таймстамп PE обнулён); пересборка в dev-окружении
Nix даёт тот же хеш. Один бинарь покрывает **NLCN37/38/39** — эти сборки несут
байт-в-байт одинаковый браузер форм, поэтому отдельного бинаря под версию нет
(проверено на NLCN38WW и NLCN39WW).

## Сборка из исходников

```bash
cd patcher
nix develop ..        # или `nix develop` из корня репозитория, затем cd patcher
make
sha256sum BiosAdvancedPatch.efi
```

Требования (всё в `flake.nix`): `gnu-efi`, GNU `binutils` (`ld`, `objcopy`),
`gcc`. `Makefile` компилирует под SysV ABI и доверяет трансляцию точки входа UEFI
(MS-ABI) файлу `crt0-efi-x86_64.o` — не добавляйте `-mabi=ms`. `.rodata` явно
копируется в PE (`objcopy -j .rodata`); без неё приложение печатает мусор.

## Запуск

Нужен **UEFI Shell**. Либо в прошивке он встроен (ищите пункт «UEFI Shell» /
«EFI Shell» в меню загрузки), либо положите `shellx64.efi` на флешку и загрузите
его. Подробно — [вход в UEFI Shell](08-uefi-shell-access.md).

В шелле:

```
Shell> fs0:                       # переключиться на флешку (может быть fs1:, fs2:, …)
FS0:\> connect -r                 # привязать драйверы, чтобы браузер форм был загружен
FS0:\> BiosAdvancedPatch.efi      # применить рантайм-патч
FS0:\> SetupUtilityApp.efi        # запустить штатную настройку — вкладка Advanced на месте
```

`connect -r` важен: патч нацелен на *резидентный* драйвер браузера форм, поэтому
тот должен быть уже загружен. При холодной загрузке в шелл он обычно загружен, но
`connect -r` это гарантирует.

## Как выглядит успех

`BiosAdvancedPatch.efi` печатает примерно:

```
=== BiosAdvancedPatch v5 ===
Verified: ThinkBook G6+ AHP  NLCN37/38/39 (same form browser).
Locating the form browser and disabling the Advanced-tab filter.

FormBrowser2 at 0x...., SendForm=0x....
Form browser image: base=0x.... size=0x....
[site] +0x03170c : cmp [reg+10],0 ; 74 de (jcc)
       after: 90 90  -> OK (verified)

Structural sites: 1   patched OK: 1

[DONE] Advanced filter disabled for this boot.
       Now run SetupUtilityApp; the Advanced tab will appear.
```

Строка **`after: 90 90 -> OK (verified)`** — доказательство, что запись физически
прошла (приложение читает байты обратно). Затем `SetupUtilityApp.efi` открывает
настройку с видимой вкладкой Advanced рядом с Main/Security/Boot.

## Диагностика — читайте вывод приложения

| Что напечатано | Значение / действие |
|---|---|
| `after: 90 90 -> OK` **и** Advanced виден | Успех. |
| `FormBrowser2 not found` | Браузер не загружен. Сначала `connect -r`, либо грузите шелл с холодного старта. |
| `Structural sites: 0`, затем fallback ничего не находит | В этой сборке нет фильтра (Advanced может быть уже виден), либо это модель, под которую идиома не подходит — прогоните `analyze_formbrowser.py` на её `H2OFormBrowserDxe` ([док 3](03-reproduce-analysis.md)). |
| `after: 74 de -> FAILED (write-protected)` | Прошивка заблокировала запись даже после ремапа атрибутов памяти и обхода `CR0.WP`. Сообщите об этом; нужна другая точка внедрения. |
| `OK (verified)`, но Advanced всё ещё нет | Байтовый патч верен, но на этой сборке фильтр достигается иначе — зафиксируйте вывод и версию прошивки для углублённого разбора. |

## Заметки

- Патч **временный**: исчезает после любой перезагрузки/сброса. Запускайте
  `BiosAdvancedPatch.efi` каждый раз, когда нужна вкладка Advanced.
- Он пишет только в копию драйвера в ОЗУ. Флеш не трогается никогда.
- `SetupUtilityApp` остаётся **немодифицированной**; `SendForm` по-прежнему
  вызывается с нулевым FormSetGuid — обычный путь кода.
