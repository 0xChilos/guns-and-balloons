# Дизайн-документація Guns and Balloons

Модульний «живий» GDD: замість одного великого документа — короткий огляд гри і окрема сторінка на кожну фічу.

## З чого почати
1. [Огляд гри](overview.md) — концепт, стовпи дизайну, ігровий цикл, команда. Читати першим.
2. Сторінки фіч у [features/](features/) — по одній на механіку.
3. [Журнал рішень](decisions.md) — що і коли вирішили, щоб не повертатися до обговорених питань.

## Зміст
| Сторінка | Про що | Власник |
| --- | --- | --- |
| [overview.md](overview.md) | Концепт, стовпи, цикл, скоуп | Всі |
| [features/balloon-controls.md](features/balloon-controls.md) | Керування кулею: панель, нахил, гальмування, інерція | Сокін |
| [features/engine-stove.md](features/engine-stove.md) | Двигун-пічка, паливо і температурні зони | Ростік |
| [features/weapons.md](features/weapons.md) | Дієгетична зброя, перезарядка, балістика | Сокін |
| [features/grappling-hook.md](features/grappling-hook.md) | Граплінг-хук і збір ресурсів | Сокін |
| [features/fire-extinguisher.md](features/fire-extinguisher.md) | Вогнегасник: запас на лічильнику, піна в стилі PS1 | Сокін |
| [features/wrench.md](features/wrench.md) | Гайковий ключ: взяти, крутити на ЛКМ (поки лише анімація) | TBD |
| [features/crosshair.md](features/crosshair.md) | Динамічний приціл в арт-стилі, інерція | Сокін |
| [features/nemesis-airship.md](features/nemesis-airship.md) | Дирижабль-переслідувач і його атаки | Вано |
| [features/biomes.md](features/biomes.md) | Рівні-біоми та їхні небезпеки | TBD |
| [features/safe-zone.md](features/safe-zone.md) | Безпечна зона між рівнями: ремонт, магазин, апгрейди | TBD |
| [features/ui-menus.md](features/ui-menus.md) | Головне меню, навігаційна панель | Ростік |
| [art-style.md](art-style.md) | Візуальний стиль і пост-обробка | Ростік |
| [tech/player-architecture.md](tech/player-architecture.md) | PlayerController, єдиний Pawn та маршрутизація взаємодій | Сокін |
| [tech/weapons.md](tech/weapons.md) | Technical reference for agents: gun C++ (revolver, rifle, shotgun), viewmodel, how to add guns and hook events | Сокін |
| [tech/grappling-hook.md](tech/grappling-hook.md) | Technical reference for agents: hook C++ (pressure, flight, world-space head, rope, pickups, debug menu) | Сокін |
| [tech/fire-extinguisher.md](tech/fire-extinguisher.md) | Technical reference for agents: extinguisher C++ (contents, counter drums, spray states, foam) | Сокін |
| [tech/wrench.md](tech/wrench.md) | Technical reference for agents: wrench C++ (states, Fix rest poses), FBX take split and import | TBD |
| [tech/crosshair.md](tech/crosshair.md) | Technical reference for agents: crosshair HUD, material, spread and inertia | Сокін |
| [tech/weapon-reload.md](tech/weapon-reload.md) | Technical reference for agents: ammo supplies and the reload state machines | Сокін |
| [tech/balloon-flight.md](tech/balloon-flight.md) | Technical reference for agents: balloon flight C++ API, how to connect controls | Сокін |
| [tech/BalloonLevel/HANDOFF.md](tech/BalloonLevel/HANDOFF.md) | Procedural level generation: M6 handoff, verification and implementation limits | TBD |
| [tech/wing-lever.md](tech/wing-lever.md) | Technical reference for agents: wing lever, drag/grab, detents, slip API | Сокін |
| [tech/stove.md](tech/stove.md) | Technical reference for agents: stove, fuel buckets, health, flight, animation and validation | Ростік |
| [source/directors-cut.md](source/directors-cut.md) | Сирі нотатки (оригінал, не редагувати) | — |

## Правила ведення
- **Одна фіча — одна сторінка, максимум ~1 сторінка тексту.** Коротше — краще. Нова фіча = копія [_template.md](_template.md).
- **Пиши правила, а не історію.** Обговорення — у PR/чаті; у документ — лише результат. Невирішене — у «Відкриті питання».
- **Числа — в таблицю «Параметри».** Невідоме значення = `TBD`, не вигадуй.
- **Статус у шапці** (`Draft` → `Agreed` → `In progress` → `Done`) і дата оновлення.
- **На сторінках `features/` — лише затверджене.** Нова фіча чи зміна спершу записується в [decisions.md](decisions.md) зі статусом `Запропоновано`.
- **Після апруву** — змінити статус на `Прийнято` і перенести на сторінку фічі (або створити нову з шаблону).
- **Зміни — через git:** гілка `docs/<тема>`, коміт `docs: <що змінено>`, PR.
- Терміни: українською, назви класів/ассетів Unreal — англійською в `коді`.

## Шаблон
Структура сторінки базується на підходах **One-Page Design** (Stone Librande, GDC 2010) і **модульного GDD** (Tim Ryan, «The Anatomy of a Design Document»): короткий огляд + окремі сторінки фіч з однаковою структурою (Навіщо → Як працює → Взаємодія → Параметри → Залежності → Відкриті питання).
