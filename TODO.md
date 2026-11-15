# GuiKit — TODO

## Demo Studio: Унификация контролов в панели Inspector

Цель: устранить визуальный разнобой в узком сайдбаре инспектора (`~260 dp`), чтобы форма контрола однозначно отражала семантику свойства (булев флаг, переключатель режима, упорядоченная шкала, выпадающий список, числовая величина).

### 1. Правила выбора контролов в `INSPECTOR`

| Контрол | Семантическое правило | Примеры |
| :--- | :--- | :--- |
| **`Switch`** | **Только истинный `bool` (`Вкл / Выкл`)** — включить/скрыть фичу или состояние. Запрещено маскировать под `Switch` выбор из двух равноправных альтернатив (`Left vs Right`, `Linear vs Log`). | `Disabled`, `Read Only`, `Word Wrap`, `Loading Spinner`, `Show Grid`, `Live Simulation` |
| **`Segmented`** | **2–4 взаимоисключающих режима без порядка «меньше → больше»**, чьи короткие подписи помещаются в одну строку (`≤ ~24 символов` суммарно). Без полосы прогресса. | `Horizontal \| Vertical`, `Leading \| Trailing`, `Left \| Right`, `Auto \| Fit \| Stretch \| Fixed`, `Start \| Center \| End`, `Date \| DateTime \| Time`, `Blink \| Solid \| Off` |
| **`StepSlider`** | **Строго упорядоченные дискретные шкалы (`меньше → больше`)**, минимум **3 шага**, где заполнение трека слева направо реально означает рост размера, уровня или степени. | `Size` (`A — XS — SM — MD — LG — XL`), `Heading Level` (`H1..H6`), `Layout Density` (`Compact — Standard — Comfortable`), `STFT Window` (`256 — 512 — 1024 — 2048`) |
| **`Select`** | **Категориальные списки из `5+` вариантов** ИЛИ **`3–4` варианта с длинными названиями**, которые теснятся в `Segmented`. Полные читаемые имена без обрубков (`Stack/Btm/Lead/Trail`, `Pri/Suc/Wrn`). | `Element Layout` (6), семантические палитры `Color` / `Variant` / `Background` (8–9), `ObjectFit` (5), `Toast Placement` (6), `Select / ComboBox / Autocomplete / MultiSelect`, `Language Preset`, `Colormap`, `Dataset` |
| **`Slider`** | **Непрерывные и шаговые числовые величины** с единицами измерения. | `Width (dp)`, `Height (dp)`, `Corner Radius`, `Border Width`, `Background Tint`, `Opacity` |

### 2. Принятые решения по пограничным случаям (для максимальной консистентности)

- [x] **Семантические цвета и варианты (`Color`, `Variant`, `Background`, `Border Color`, `Badge Variant` — 8–9 токенов)** → перевести с `StepSlider` на **`Select`** с полными именами (`Default`, `Primary`, `Secondary`, `Success`, `Warning`, `Danger`, `Info`, `Ghost`), убрав ложную полосу заполнения и трёхбуквенные сокращения.
- [x] **Булевы флаги оверлеев в сценах** → унифицировать все `bool`-переключатели внутри панели `INSPECTOR` на **`Switch`** (оставив `Checkbox` только внутри самого превью: ячейки `Table`, узлы `TreeView` и т.п.).
- [x] **`Heading Level` (`H1..H6`)** → оставить на **`StepSlider`** как иерархическую типографическую шкалу.

---

### 3. Чеклист миграции по файлам (`examples/demo/`)

#### Общие секции инспектора (`examples/demo/demo_context.hpp`)
- [x] `ELEMENT`: `Label Color`, `Description Color`, `Badge Variant` → `Select` (полные названия `UiVariant`).
- [x] `ELEMENT LAYOUT`:
  - `Layout` (`Auto`, `Stacked`, `Top`, `Bottom`, `Leading`, `Trailing`) → `Select`.
  - `Alignment` (`Start`, `Center`, `End`) → `Segmented`.
  - `Width` (`Auto`, `Fit`, `Stretch`, `Fixed`) → `Segmented` (визуально отделяет режим от `Slider` `Width (dp)` ниже).
- [x] `CONTROL LAYOUT`:
  - `Width` (`Auto`, `Fit`, `Stretch`, `Fixed`) → `Segmented`.
  - `Height` (`Auto`, `Fit`, `Stretch`, `Fixed`) → `Segmented`.
  - `Alignment` (`Start`, `Center`, `End`) → `Segmented`.
- [x] `CONTROL STYLE`:
  - `Size` (`A..XL`) → оставить `StepSlider`.
  - `Color`, `Background`, `Border Color` → `Select`.
- [x] `Layout Density` (`Compact`, `Standard`, `Comfortable`) → оставить `StepSlider`.

#### Экраны `ELEMENTS` (`examples/demo/elements/`)
- [x] `element_screen.cpp`: синхронизировать с `demo_context.hpp` (`Layout` → `Select`, `Alignment` / `Width` → `Segmented`, цветовые варианты → `Select`, `Size` → `StepSlider`).
- [x] `heading_screen.cpp`: `Heading Level` (`H1..H6`) → оставить `StepSlider`; `Heading Layout` (`Stacked | Overline | Inline`), `Separator` (`None | Bottom | Trailing`), `Alignment` (`Start | Center | End`), `Heading Width` (`Auto | Fit | Stretch | Fixed`) → `Segmented`; цветовые варианты → `Select`.
- [x] `button_screen.cpp`: `Icon Placement` (`Leading | Trailing`) → `Segmented` (вместо 2-точечного `StepSlider`).
- [x] `segmented_screen.cpp`: `Icon Placement` (`Leading | Trailing`) → `Segmented`.
- [x] `slider_screen.cpp`: `Track Control Mode` (`Slider | Range | Step | Progress`) → `Segmented` (или `Select`, если подписи длинные); `Value Placement` → `Segmented` / `Select`.
- [x] `level_meter_screen.cpp`: `Channel Configuration` (`Mono | Stereo | 8-Ch`) → `Segmented`; `Orientation` (`Horizontal | Vertical`) → `Segmented`.
- [x] `knob_screen.cpp`: `Value Placement` → `Segmented` (если ≤ 4 коротких) или `Select`.
- [x] `input_screen.cpp` & `spin_box_screen.cpp`: `Cursor Blink Mode` (`Blink | Solid | Off`) → `Segmented`.
- [x] `tag_input_screen.cpp`: `Tag Variant` → `Select`.
- [x] `select_screen.cpp`: `Control Mode` (`Select`, `ComboBox`, `Autocomplete`, `MultiSelect`) → `Select` (4 длинных слова не теснятся в узком сайдбаре).
- [x] `radio_screen.cpp`: `Active Selection` (`Option A | B | C`) → `Segmented`.
- [x] `image_screen.cpp`: `ObjectFit Policy` (5 режимов) → `Select`; `Horizontal / Vertical Fit Align` (`Start | Center | End`) → `Segmented`.

#### Экраны `PANELS` & `CONTENT` (`examples/demo/panels/`, `examples/demo/content/`)
- [x] `breadcrumb_screen.cpp`, `pagination_screen.cpp`, `card_screen.cpp`: цветовые/фоновые варианты → `Select`; `Size` → оставить `StepSlider`.
- [x] `overlays_screen.cpp`: `Stack Placement` (`Toast`, 6 позиций) → `Select`.
- [x] `sidebar_screen.cpp`: `Menu Style`, `Dock Side` (`Left | Right`), `ActivityBar Style` → `Segmented`.
- [x] `tab_bar_screen.cpp`: `TabBar Style` (`Underline | Pills | Cards | Folder`) → `Segmented` (или `Select`).
- [x] `table_screen.cpp`: `Table Mode` → `Segmented` / `Select` по длине подписей.
- [x] `tree_view_screen.cpp`: `Selection Style`, `Hierarchy Guides` → `Segmented`.
- [x] `code_editor_screen.cpp`: `Language Preset` → `Select`; `Debug Toolbar Position` → `Segmented` / `Select`.

#### Экраны `SCENES` (`examples/demo/scenes/`)
- [x] `schematic_screen.cpp`: `Canvas Backdrop` (`Dots | Lines | Solid`) → `Segmented`.
- [x] `signal_screen.cpp`: `Panel Stack Order`, `Waveform Amplitude Scale` (`Linear | dB`), `Spectrogram Frequency Scale` (`Linear | Log`) → `Segmented`; `FFT Window Function` → `Select`; `STFT Window Size` (`256..2048`) → оставить `StepSlider`.
- [x] `image_view_screen.cpp`: `Histogram Scale` (`Linear | Log`) → `Segmented`; `"Sidebar on Right"` (`Switch`) → заменить на `Sidebar Side` (`Left | Right`) через `Segmented`.
- [x] `bpmn_screen.cpp`: `Theme` и `Edge Routing` → `Segmented` (или `Select` при длинных подписях).
- [x] `sonar_screen.cpp`, `waterfall_screen.cpp`, `attitude_screen.cpp`, `car_dashboard_screen.cpp`:
  - Короткие бинарные `Select` на 2 варианта (`km/h | mph` и аналогичные) → `Segmented`.
  - Чеклисты оверлеев в инспекторе (`Checkbox`) → унифицировать на `Switch`.
