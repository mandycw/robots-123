## AstroBots

AstroBots is a simple space-combat sandbox where each ship is controlled by a tiny domain-specific language (DSL). You “program” a ship by writing a `SetupShip()` function that emits a sequence of opcodes (instructions). Every turn, the game runs that opcode sequence to decide the ship’s actions.

The core DSL and interpreter live in:

- `classes/AstroBots.h` (the user-facing DSL macros)
- `classes/AstroBots.cpp` (the VM/interpreter)
- `classes/AstroTypes.h` and `classes/AstroArena.h/.cpp` (opcodes, costs, and gameplay rules)

## The idea of the game

- **Arena**: a \(2048 \times 2048\) world that **wraps at the edges** (a torus).
- **Ships**: start in a circle, each with HP and fuel.
- **Asteroids**: drift around. Shooting/breaking them can indirectly help you survive (fuel mechanics).
- **Win condition**: the simulation ends when there is **one ship left alive**, or it reaches the turn limit.

Your ship code does not directly set position. Instead it uses actions like thrust, turning, scanning, and firing weapons.

## How the ship “program” works

### One-time “compile”

Each ship implements:

- `int SetupShip()` to build its `code` (a `std::vector<int>`)
- `Finalize()` appends `END` and returns the script cost

The game logs `script cost X/30` for each ship at startup.

### Turn-by-turn execution

Each turn:

- The arena resets certain per-turn state (cooldowns tick down, scan results are cleared).
- Every alive ship runs its program from the start (opcode 0) until `END`.

Important consequences:

- **Scan results do not persist**: `scan_hit` is cleared at the start of every turn. If you want to react to what you “see”, you generally must `SCAN()` earlier in the same turn.
- **Signals do not persist**: `signal` is reset each turn (it is currently more of a hook/visual than a full communication system).

## Script budget: “30 total opcodes”

In this codebase, the number **30** is the maximum **script cost budget** (`ASTRO_MAX_SCRIPT_COST = 30`), not the number of opcode types.

- Each DSL action has a cost (for example, `THRUST` costs 2, `FIRE_PHOTON` costs 4).
- Conditionals and flow-control are part of the bytecode but are *not* charged as action cost by the DSL macros.

So when people say “30 total opcodes”, what the engine enforces is: **your `SetupShip()` program should cost \(\le 30\)**.

## DSL quickstart (what you write)

You write C++ that uses the DSL macros inside `SetupShip()`:

```cpp
int MyShip::SetupShip() {
    SCAN();
    IF_SEEN() {
        TURN_TO_SCAN();
        IF_SCAN_LE(500) {
            IF_SHIP_CAN_FIRE_PHASER() { FIRE_PHASER(); }
            IF_SHIP_CAN_FIRE_PHOTON() { FIRE_PHOTON(); }
        }
        THRUST(2);
    } ELSE() {
        THRUST(4);
    }
    return Finalize();
}
```

## Opcode / instruction reference (all available opcodes)

The interpreter is a small switch statement in `ShipBase::Run()`. The opcodes are defined in `classes/AstroTypes.h`.

### Actions

- **`WAIT`** (`ASTRO_OP_WAIT`)
  - Does nothing.
  - DSL macro: `WAIT_()`

- **`THRUST power`** (`ASTRO_OP_THRUST`, parameter)
  - Accelerates the ship forward in the direction it is currently facing.
  - DSL macro: `THRUST(P)` where `P` is stored as `(int)(P * 10)` and reconstructed as `P/10.0` at runtime.
  - Gameplay notes:
    - Fuel cost: `power * THRUST_FUEL_COST` (see `classes/AstroTypes.h`).
    - If fuel is low/empty, thrust is reduced (never fully zeroed).
    - Velocity is clamped to `MAX_VELOCITY`.

- **`TURN_DEG degrees`** (`ASTRO_OP_TURN_DEG`, parameter)
  - Sets your ship’s **target heading** (absolute degrees 0–360), not a relative “turn by”.
  - The ship rotates toward `targetAngle` at `ROTATION_SPEED` degrees per turn.
  - DSL macro: `TURN_DEG(D)`

- **`FIRE_PHASER`** (`ASTRO_OP_FIRE_PHASER`)
  - Fires a hitscan ray out to `PHASER_RANGE`.
  - If it hits a ship: deals `PHASER_DAMAGE` and can destroy the ship at 0 HP.
  - If it hits an asteroid: breaks/damages it and grants a small fuel reward.
  - Has a cooldown (`PHASER_COOLDOWN` turns).
  - DSL macro: `FIRE_PHASER()`

- **`FIRE_PHOTON`** (`ASTRO_OP_FIRE_PHOTON`)
  - Spawns a photon torpedo moving forward at `PHOTON_SPEED` (plus your current ship velocity).
  - Torpedoes have `PHOTON_LIFETIME` and deal `PHOTON_DAMAGE` on hit.
  - Has a cooldown (`PHOTON_COOLDOWN` turns).
  - DSL macro: `FIRE_PHOTON()`

- **`SCAN`** (`ASTRO_OP_SCAN`)
  - Finds the closest **ship or asteroid** within `ASTRO_SCAN_RANGE`.
  - Sets:
    - `scan_hit` (boolean)
    - `scan_dist` (distance to target)
    - `scan_angle` (absolute angle to target, 0–360)
  - DSL macro: `SCAN()`

- **`SIGNAL value`** (`ASTRO_OP_SIGNAL`, parameter)
  - Sets the ship’s `signal` value for this turn and records its position into `signals`.
  - Currently, scanning does not use signals; think of this as a minimal communication/visual hook.
  - DSL macro: `SIGNAL(V)`

- **`TURN_TO_SCAN`** (`ASTRO_OP_TURN_TO_SCAN`)
  - If `scan_hit` is true, sets `targetAngle = scan_angle`.
  - DSL macro: `TURN_TO_SCAN()`

### Conditions (set the VM “flag”)

Conditions do not directly branch; they just compute a boolean `flag`. Branching is done by `JUMP_IF_FALSE` (which is inserted automatically by the `IF_*` DSL macros).

- **`IF_SEEN`** (`ASTRO_OP_IF_SEEN`)
  - `flag = scan_hit`
  - DSL macro: `IF_SEEN() { ... }`

- **`IF_SCAN_LE range`** (`ASTRO_OP_IF_SCAN_LE`, parameter)
  - `flag = (scan_hit && scan_dist <= range)`
  - DSL macro: `IF_SCAN_LE(R) { ... }`

- **`IF_DAMAGED`** (`ASTRO_OP_IF_DAMAGED`)
  - `flag = (hp < ASTRO_START_HP)`
  - DSL macro: `IF_SHIP_DAMAGED() { ... }`

- **`IF_HP_LE hp`** (`ASTRO_OP_IF_HP_LE`, parameter)
  - `flag = (hp <= value)`
  - DSL macro: `IF_SHIP_HP_LE(N) { ... }`

- **`IF_FUEL_LE fuel`** (`ASTRO_OP_IF_FUEL_LE`, parameter)
  - `flag = (fuel <= value)`
  - DSL macro: `IF_SHIP_FUEL_LE(N) { ... }`

- **`IF_CAN_FIRE_PHASER`** (`ASTRO_OP_IF_CAN_FIRE_PHASER`)
  - `flag = (phaser_cooldown == 0)`
  - DSL macro: `IF_SHIP_CAN_FIRE_PHASER() { ... }`

- **`IF_CAN_FIRE_PHOTON`** (`ASTRO_OP_IF_CAN_FIRE_PHOTON`)
  - `flag = (photon_cooldown == 0)`
  - DSL macro: `IF_SHIP_CAN_FIRE_PHOTON() { ... }`

### Flow control

These are primarily emitted by the `IF_*` / `ELSE()` DSL helpers:

- **`JUMP target`** (`ASTRO_OP_JUMP`, parameter)
  - Unconditional jump (sets the program counter to `target`).

- **`JUMP_IF_FALSE target`** (`ASTRO_OP_JUMP_IF_FALSE`, parameter)
  - If the current `flag` is false, jump to `target`.

- **`END`** (`ASTRO_OP_END`)
  - Stops execution of the ship program for the current turn.

## Costs (the budget system)

Action costs are defined in `classes/AstroTypes.h`:

- `WAIT`: 0
- `TURN_DEG`: 1
- `SCAN`: 1
- `SIGNAL`: 1
- `THRUST`: 2
- `FIRE_PHASER`: 3
- `FIRE_PHOTON`: 4

These costs add up during `SetupShip()` and are logged. If your ship exceeds the 30-point budget, it will still run, but the log will mark it as exceeding the limit.

## Tips for writing a good bot

- **Always scan before reacting**: `SCAN()` early, then use `IF_SEEN()` / `IF_SCAN_LE(...)`.
- **Turn is smooth**: `TURN_DEG` and `TURN_TO_SCAN` set a target angle; rotation takes time.
- **Manage cooldowns**: check `IF_SHIP_CAN_FIRE_*()` before firing to avoid wasted instructions.
- **Asteroids are resources and hazards**: collisions hurt; breaking asteroids can lead to fuel pickups.
