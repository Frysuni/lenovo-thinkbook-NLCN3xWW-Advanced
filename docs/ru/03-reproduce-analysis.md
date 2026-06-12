# 3. Воспроизведение анализа (для любой прошивки / модели)

Так можно — с нуля — подтвердить, что данный BIOS прячет вкладку Advanced
способом из [первопричины](02-root-cause.md), и вывести точный патч для сборки,
которой нет в [матрице](05-cross-version-matrix.md).

Всё ниже выполняется в dev-окружении Nix:

```bash
nix develop          # даёт UEFITool, capstone, python3, radare2, binutils, …
```

## Шаг 1 — получить образ прошивки

Скачайте пакет BIOS для своей модели (см.
[загрузку прошивок](07-firmware-downloads.md)) и достаньте сырой SPI-образ.
Пакеты Lenovo `NLCN` — это инсталляторы Insyde `H2OFFT`; прошиваемый образ — это
большой `.bin`/`.FD`/`.cap` внутри. Если у вас уже есть полный дамп SPI,
используйте его.

## Шаг 2 — извлечь `H2OFormBrowserDxe`

Фильтр существует только в **распакованном** драйвере. Найти его сканированием
сырого `.bin` нельзя (драйвер лежит внутри сжатого тома прошивки).

Используйте UEFITool (GUI) или `uefiextract` (CLI, в dev-окружении):

```bash
uefiextract NLCN38WW.bin            # создаёт NLCN38WW.bin.dump/
```

Затем найдите модуль по GUID `9E5DAEB4-4B91-4466-9EBE-81C7E4401E6D` и возьмите
его **«PE32 image section» → `body.bin`**. В дереве дампа это:

```
…/153 H2OFormBrowserDxe/2 PE32 image section/body.bin
```

> Подсказка: `find NLCN38WW.bin.dump -path '*H2OFormBrowserDxe*PE32*body.bin'`

## Шаг 3 — найти фильтр и вывести патч

```bash
python3 patcher/analyze_formbrowser.py \
    'NLCN38WW.bin.dump/…/153 H2OFormBrowserDxe/2 PE32 image section/body.bin'
```

Пример вывода:

```
=== …/body.bin  (244512 bytes) ===
  Found 1 filter site(s):

  [filter @ file offset 0x03170c]
    cmp byte [rbx+0x10], 0 ; jz -34
    0x03170c: cmp     byte ptr [rbx + 0x10], 0  <== filter
    0x031710: je      0x3170c... (назад)
    0x031712: mov     rcx, qword ptr [rbx]
    …
    search anchor (9 bytes) : ff 50 48 80 7b 10 00 74 de
    patch the J-byte at off : 0x031710  (74 de  ->  90 90)

Summary: filter PRESENT (patchable)
```

Толкование:

- **`Found N filter site(s)`** → эта сборка прячет FormSet с флагом = 0. Патчите
  J-байт(ы), которые она указывает (`74/75 xx → 90 90`).
- **`No 'flag==0 -> skip' filter found`** → эта сборка **не** прячет Advanced;
  патчить нечего (он должен быть уже виден).

Инструмент сопоставляет идиому *структурно* (`cmp byte [reg+0x10],0 ; j(n)z
назад`), поэтому терпим к другому регистру или смещению перехода на иных сборках.
Если он сообщает **более одного** места, дизассемблируйте вокруг каждого (он
печатает окно) и патчите то, что внутри процедуры сортировки FormSet.

## Шаг 4 — (опционально) подтвердить таблицу флагов

Чтобы независимо убедиться, что Advanced помечен «скрыть», осмотрите таблицу по
офсету из анализа (`0x37c30` на NLCN38). Каждая запись — 16-байтовый GUID FormSet
плюс 4-байтовый флаг; GUID Advanced `C6D4769E-7F48-4D2A-98E9-87ADCCF35CCC` несёт
флаг `0`. Также можно выгрузить IFR настройки **Universal IFR Extractor**
(внешний, опционально), чтобы увидеть определение FormSet Advanced:

```
Form Set: Advanced [C6D4769E-7F48-4D2A-98E9-87ADCCF35CCC], …
  VarStore: … Name: SystemConfig
  Form: Advanced, FormId: 0x1
```

## Шаг 5 — убедиться, что рантайм-патчер его найдёт

Рантайм `BiosAdvancedPatch.efi` использует тот же сопоставитель идиомы, что и
оффлайн-инструмент, но ограниченный образом браузера форм, который он находит во
время выполнения. Если оффлайн-инструмент сообщает ровно одно место для вашей
сборки, рантайм-патчер найдёт и заменит его на NOP (и печатает байты до/после,
чтобы вы могли проверить на машине).

## Использованные внешние инструменты

- **UEFITool / uefiextract** — извлечение томов прошивки. Есть в dev-окружении
  Nix (`pkgs.uefitool`).
- **capstone** — дизассемблер для `analyze_formbrowser.py`. В dev-окружении.
- **Universal IFR Extractor** (опционально) — человекочитаемые дампы FormSet/IFR.
  Для патча не нужен; берите из его апстрим-проекта при желании.
