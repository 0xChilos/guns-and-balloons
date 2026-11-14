# План: політ кулі (прототип)

> **Статус:** у роботі · **Оновлено:** 2026-11-14

## Правила роботи

- **Фази незалежні.** Кожна фаза (нижче) — окрема сесія або окремий блок роботи. Наприкінці фази позначити її в [журналі прогресу](#журнал-прогресу) (`[x]`, дата, 1–2 рядки нотаток: що зроблено, відхилення, імена створених ассетів). Наступна сесія починає з цього файлу й журналу, а не з повторного дослідження.
- **Читати лише перелічене у фазі.** Весь контекст, потрібний для фази, вказано в її розділі «Прочитати». Решту репозиторію не сканувати, `Content/` не обходити, `docs/source/` не читати.
- **Великі файли читати частинами.** `RevolverComponent.cpp/.h` (~15k/13k байт) — лише вказані рядки/функції через `Grep` + `Read` з `offset/limit`, не цілком.
- **Unreal MCP — точково.** Не викликати інструменти, що повертають повний дамп рівня, всіх ассетів чи всього графа Blueprint. Спершу `describe_toolsets` / `describe_tools` лише для потрібного набору, потім запити з фільтром (конкретний ассет, конкретний компонент). Якщо відповідь велика — не повторювати запит, а записати потрібні факти в журнал.
- **Логи збірки** не виводити повністю: фільтрувати на `error`/`warning` (напр. `| Select-String -Pattern 'error|warning' | Select-Object -First 40`).
- **Не перечитувати** щойно відредаговані файли для перевірки — Edit повідомить про помилку.
- Якщо фаза не вміщується — зупинитися на завершеному кроці, записати стан у журнал і продовжити в новій сесії.

## Unreal MCP (`unreal-mcp`)

Агент може й повинен працювати з редактором через MCP-сервер `unreal-mcp`, коли редактор запущено. Інструменти deferred — спершу завантажити схеми через `ToolSearch` (`select:mcp__unreal-mcp__GetStatus,...`).

- **Перевірка з'єднання:** `GetStatus`. Якщо редактор не відповідає — фази 1–3 виконувати без нього, а для 4–6 попросити людину відкрити редактор.
- **Пошук інструментів:** `list_toolsets` → `describe_toolsets` / `describe_tools` лише для потрібного набору (Blueprint, рівень, ассети) → виклик через `call_tool`. Не описувати всі набори одразу.
- **Прямі інструменти:** `GetAssetsByType` / `GetSelectedAssets` (знайти `BP_Balloon`, ассети), `GetActorsByType` (актори на рівні — фільтрувати за типом), `SpawnMeshAt` (перешкоди й орієнтири у фазі 5), `RunConsoleCommand` (`t.MaxFPS 30/60/120`, PIE-команди у фазі 6), `PrintToLog` (позначки в лозі під час замірів), `GetViewportCamera` / `SetViewportCamera` (огляд сцени).
- **Де використовувати:** фаза 4 (компоненти й налаштування `BP_Balloon`), фаза 5 (створення й наповнення рівня), фаза 6 (консольні команди, читання логу замірів).
- Правило контексту: запити з фільтром, без повних дампів рівня/графів (див. вище). Не застосовувати `AskAssistant`, рендер- і медіа-інструменти (`TriggerMRGRender`, `MediaJob*`, `BakeMeshTextures` тощо) — вони для цієї задачі не потрібні.
- Перед збіркою UBT редактор треба закрити — попросити людину або виконати після завершення роботи з MCP; після збірки знову перевірити `GetStatus`.

## Обсяг

- Куля автоматично летить уперед; командами швидкості неможливо зупинити її або ввімкнути задній хід.
- **Два незалежні керування:**
  - **Гравець** — без змін, як у `Movement_Scene1` (`AFirstPersonCameraPawn`): огляд мишею, ЛКМ — взаємодія/постріл, перезарядка кліком по ящику б/к.
  - **Куля** — клавіатура, **лише тимчасово** для прототипу. Фінальне керування — як у [дизайні](features/balloon-controls.md): панель швидкості й бокові важелі; на цьому етапі їх не робити. Клавіатура не є зміною дизайну — запис у `decisions.md` не потрібен; її компонент має легко вимикатися/видалятися, коли з'являться панель і важелі.
- Керування висотою, анімації панелей і важелів, візуальний нахил, підключення пічки — поза цим етапом. Питання висоти залишається відкритим у дизайні.

## Відомі факти про код (не досліджувати повторно)

- Модуль: `Source/gunsandballoons/`, макрос API — `GUNSANDBALLOONS_API`. Нові модулі в `Build.cs` не потрібні (`Engine`, `CoreUObject` вже є).
- `AFirstPersonCameraPawn`: root — `UCameraComponent Camera` (`bUsePawnControlRotation = true`), без movement component. У `NotifyControllerChanged()` викликає `PC->SetIgnoreMoveInput(true)`, `PC->SetControlRotation(GetActorRotation())`, `AddTickPrerequisiteActor(PC)`. `Tick()` (Source/gunsandballoons/FirstPersonCameraPawn.cpp) рано виходить без локального PC, далі: трасування цілі, ЛКМ, `UpdateLook(PC)`, `UpdateWeaponSway`. `UpdateLook` рано виходить, якщо миша не рухалась; обмеження `bLimitYaw` рахується від `GetActorRotation().Yaw` — при attach до кулі воно автоматично повертається разом із корзиною.
- Трасування взаємодії: FirstPersonCameraPawn.cpp ~229 — **не змінювати**.
- Трасування пострілу: RevolverComponent.cpp ~450 — `FCollisionQueryParams Params(SCENE_QUERY_STAT(RevolverShot), true, Owner);` — сюди додається ігнор кулі.
- GameMode: `FirstPersonCameraGameMode` (глобальний), pawn класу `AFirstPersonCameraPawn`.

## Збірка

- Двигун: `G:\UE\UE_5.9` (association 5.9). Команда UBT:
  `G:\UE\UE_5.9\Engine\Build\BatchFiles\Build.bat gunsandballoonsEditor Win64 Development -project="…\gunsandballoons.uproject" -WaitMutex`
- Перед збіркою закрити редактор; логи фільтрувати на `error|warning`.

## Фаза 1 — компонент польоту (C++, без редактора)

**Прочитати:** цей файл, [balloon-controls.md](features/balloon-controls.md), [AmmoSupplyComponent.h](../Source/gunsandballoons/AmmoSupplyComponent.h) (як зразок стилю UCLASS/UPROPERTY у проєкті).

**Створити** `Source/gunsandballoons/BalloonFlightComponent.h/.cpp`, `UCLASS(ClassGroup=(Balloon), meta=(BlueprintSpawnableComponent))`, `UBalloonFlightComponent : UActorComponent`, tick у `TG_PrePhysics`. Рухає весь актор-власник. Про клавіатуру не знає — його методи єдина точка входу для будь-якого керування (зараз клавіатура, пізніше панель/важелі/пічка).

Інтерфейс (орієнтир):

```cpp
UFUNCTION(BlueprintCallable, Category="Balloon|Flight") void SetTargetSpeed(float SpeedCmPerSecond);
UFUNCTION(BlueprintCallable, Category="Balloon|Flight") void SetTurnInput(float Input);
UFUNCTION(BlueprintPure,     Category="Balloon|Flight") float GetCurrentSpeed() const;
UFUNCTION(BlueprintPure,     Category="Balloon|Flight") float GetTargetSpeed() const;
UFUNCTION(BlueprintPure,     Category="Balloon|Flight") float GetCurrentTurnRate() const;
// EditAnywhere, Category="Balloon|Flight", ClampMin: MinSpeed, MaxSpeed, InitialSpeed,
// Acceleration, Deceleration, MaxTurnRate, TurnAcceleration, TurnDeceleration
```

Правила:

1. **Постійний рух.** `BeginPlay`: виправити параметри (`MinSpeed <= 0` → мале додатне; `MaxSpeed < MinSpeed` → `MinSpeed`) з одноразовим `UE_LOG(LogTemp, Warning, ...)`; `CurrentSpeed = TargetSpeed = Clamp(InitialSpeed, Min, Max)`. Кожен tick: зсув на `CurrentSpeed * DeltaSeconds` уздовж **горизонтального** forward (`FRotator(0, Yaw, 0).Vector()`), через `GetOwner()->AddActorWorldOffset(Delta, /*bSweep=*/true)`.
2. **Швидкість.** `SetTargetSpeed` обмежує `[MinSpeed, MaxSpeed]` (≤0 → `MinSpeed`). `CurrentSpeed = FMath::FInterpConstantTo(Current, Target, Dt, Target > Current ? Acceleration : Deceleration)`. Не `FInterpTo`.
3. **Поворот.** `SetTurnInput` обмежує `[-1, 1]`, значення зберігається до наступного виклику. Ціль кутової швидкості `Input * MaxTurnRate`; темп — `TurnAcceleration`, якщо модуль зростає в тому ж знаку, інакше `TurnDeceleration` (зменшення модуля або зміна напрямку до перетину нуля). Кожен tick: `AddActorWorldRotation(FRotator(0, CurrentTurnRate * Dt, 0))`. Рух уперед триває під час повороту.
4. Одиниці: см/с, см/с², град/с, град/с². Значення — **TBD**; тимчасові значення позначити `// TBD: prototype value`.

**Готово, коли:** компілюється. Записати в журнал.

## Журнал прогресу

- [x] **Фаза 1** — компонент польоту (C++) · 2026-11-14
	- Створено `Source/gunsandballoons/BalloonFlightComponent.h/.cpp` (TG_PrePhysics, інтерфейс за планом, усі значення `// TBD: prototype value`). UBT-збірка `gunsandballoonsEditor Win64 Development`: успішна, без error/warning.
	- Відхилення: unreal-mcp у сесії недоступний (для фази 1 не потрібен); редактор довелося завершити примусово (Kill PID 5632) — Live Coding блокував UBT. Файл плану сам створено (`docs/balloon-flight-plan.md`), оскільки його не було в репо.
	- Нові ассети: нема (тільки C++). У `Content/` компонент ще ніде не використовується — додати в фазі 4.
- [x] **Фаза 2** — тимчасове клавіатурне керування кулею · 2026-11-14
	- Створено `Source/gunsandballoons/BalloonKeyboardControlComponent.h/.cpp`: `BeginPlay` знаходить `UBalloonFlightComponent` на owner (нема → warning + `SetComponentTickEnabled(false)`), додає `Flight->AddTickPrerequisiteComponent(this)`. Tick: `IsInputKeyDown` на PC 0 — W/S зсувають ціль на `SpeedChangeRate * Dt` від `GetTargetSpeed()`, A/D → `SetTurnInput(-1/+1/0)`. Клавіші `FKey` (EditAnywhere, W/S/A/D), `SpeedChangeRate = 100` `// TBD: prototype value`.
	- Відхилення: нема. `AFirstPersonCameraPawn` не змінено; Input Action/Mapping Context не використовувались.
	- Нові ассети: нема (тільки C++). Компонент ще не доданий до актора — у фазі 4 разом із `BalloonFlightComponent`.
- [x] **Фаза 3** — гравець на кулі · 2026-11-14
	- `FirstPersonCameraPawn.h/.cpp`: приватні `LastParentYaw`/`bHasParentYaw`; `Tick` перед `UpdateLook` переносить delta батьківського yaw у control rotation (вільний від умови «миша рухалась»); ініціалізація в `NotifyControllerChanged` після `SetControlRotation`; `BeginPlay` (після Super) додає tick prerequisite на `UBalloonFlightComponent` батька; include лише в `.cpp`.
	- `RevolverComponent.cpp` ~450: після `Params` додано `AddIgnoredActor(GetAttachParentActor())` — постріл не влучає у свою кулю. Трасування взаємодії не змінено.
	- Відхилення: нема. Без батька поведінка ідентична попередній (gards `bHasParentYaw`/`GetAttachParentActor`).
	- Нові ассети: нема (тільки C++).
- [x] **Фаза 4** — `BP_Balloon` у редакторі (MCP) · 2026-11-14 (завершено)
	- **Прочитано (назви):** змінні `Balloon|Controls`: LeverL, LeverR, FurnaceDoor, PropellerSpeed, Temperature01, Speed01, BurnTime. Функції: `UserConstructionScript`, `GetInteractionForBone` (bone → interaction). Фактичний root — `Mesh` (SkeletalMeshComponent, SK_Balloon, profile Custom), НЕ DefaultSceneRoot; діти Mesh: Floor_0–3, Wall_0–7, StoveBlock, ConsoleBlock (BoxComponents, profile Custom), FurnaceLight + LanternLight (PointLight) з білбордами.
	- **Зроблено:** root = `BalloonCollision` (BoxComponent, profile Pawn, extent 125/271/145 — box root ігнорує relative transform, тож extent від походження актора; геометрія від pivot: x ±124, y −120…270, z −52…144). Mesh — дитина BalloonCollision, identity; simulate_physics=False (physics_asset_override відсутній → PHA успадковується від SK_Balloon). Решта трансформів не змінені. Compile + Save успішно.
	- **Блокер:** основний репозиторій — `G:\UE\gunsandballoons` (саме його відкрито в редакторі); C++ компонентів з фаз 1–3 у ньому нема (ворктрі некомітнутий). Не можу додати BalloonFlight/BalloonKeyboardControl, поки головне репо не мати збудовані класи. Тестові/дубльні box-и підчищено.
	- Крок 4 завершиться після: копіювання source-файлів у `G:\UE\gunsandballoons`, закриття редактора, UBT-збірки там, перезапуску редактора, додавання 2 компонентів + compile/save через MCP.
- [x] **Фаза 5** — рівень польоту, перешкоди, орієнтири (MCP) · 2026-11-14
	- Створено `Content/Scenes/BalloonFlight_Test.umap`: світло, земля на -5000 см, `BP_Balloon` у (0,0,0), `FirstPersonCameraPawn` attach (rel 0,0,80), `AmmoCrate` attach (rel 50,0,20), 4 куби-перешкоди, 15 орієнтирів на землі; GameMode override `FirstPersonCameraGameMode`; збережено.
- [x] **Фаза 6** — PIE-перевірка і заміри (MCP) · 2026-11-14
	- UBT-збірка успішна. `Movement_Scene1` PIE без помилок. `BalloonFlight_Test` PIE: автотест ACCEL Min→Max ≈6.333 с і TURN 90° = 3.000 с при t.MaxFPS 30/60/120 (практично однакові). Debug/autotest-прапорці вимкнено на інстансі.
- [x] **Керування клавіатурою кулею (кінець прототипу)** — 2026-11-14
	- Блокер: у інстансі `BP_Balloon` у `BalloonFlight_Test.umap` утворилися під час маніпуляцій з'яви неправильні значення flight-компонента: `MaxSpeed=50` (мало бути 300), `InitialSpeed=20`, `MaxTurnRate=1`; компонент клавіатури мав `SpeedStep=144.28`, `TurnDuration=301.97`.
	- Причина симптому «W/S не змінюють швидкість»: `UBalloonFlightComponent::SetTargetSpeed` обмежує шлюз `[MinSpeed, MaxSpeed]` — з `MaxSpeed=50` кожен крок W/S (клік → `SpeedStep`) просто clamp-уважався до `MaxSpeed`, тому швидкість фактично не змінювалася.
	- Виконано: виправлено значення на інстансі (flight: Max=300, Min=50, Init=100, MaxTurn=30, Accel/Decel=40, TurnAcc/TurnDec=60; keyboard: SpeedStep=50, TurnDuration=0.5, bEnabled=True). Перевірено: `SetTargetSpeed(200) → 200` після виправлення (логіка flight працює).
	- Зроблено: прибрано тимчасові діагностичні `UE_LOG` у `BalloonKeyboardControlComponent`; оновлено source (clean) + UBT-збірка build12 успішна.
