# Texas Hold 'Em Odds Calculator

Computes each player's chance of winning a hand for 2–9 players whose hole cards
are known, at any street (preflop / flop / turn / river). Preflop it compares a
**Monte Carlo simulation** against **full enumeration** to show the
speed/accuracy trade-off; once the board is dealt, exact enumeration is cheap so
it just reports the exact odds.

## What it reports

Per player: **Win%** (sole winner), **Tie%** (shares the best hand in a split),
and **Equity%** (win plus fair share of split pots). It also computes each
player's **hand-category probabilities** — how often their final made hand is a
pair, two pair, straight, flush, etc. Monte Carlo results include a 95%
confidence interval on equity.

## Files

| File | Purpose |
|------|---------|
| `card.h` | Card as an int 0..51; parse/format, rank/suit helpers |
| `hand_evaluator.h/.cpp` | Rules-based best-5-of-7 evaluator → comparable score + category |
| `deck.h` | Remaining-deck computation; random sampling for Monte Carlo |
| `showdown.h` | Evaluate all players on a board; find winner(s) / splits |
| `equity.h/.cpp` | `enumerate()` (exact) and `monte_carlo()` (estimate) engines |
| `main.cpp` | CLI + interactive input, validation, table / JSON output |
| `test_evaluator.cpp` | Evaluator self-tests (categories, tiebreaks, ties) |
| `oracle_verify.cpp` | Independent cross-check (different evaluator) of equities |

## Build

Requires a C++17 compiler. On this machine that's MSVC + CMake + Ninja — run the
commands from a **Visual Studio Developer PowerShell / x64 Native Tools prompt**
so `cl` and the `INCLUDE`/`LIB` environment are set.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # runs the evaluator self-tests
```

The executables land in `build/`: `poker.exe` (CLI), `poker_server.exe` (web UI),
`test_evaluator.exe`, `oracle_verify.exe`.

Single-command alternative (no CMake):

```powershell
cl /std:c++17 /EHsc /O2 main.cpp equity.cpp hand_evaluator.cpp /Fe:poker.exe
```

## Usage

Interactive (prompts for players and board):

```powershell
.\build\poker.exe
```

Command line:

```powershell
# Preflop AA vs KK (runs enumeration + Monte Carlo comparison)
.\build\poker.exe --players AhAs KdKc

# On a flop (exact only)
.\build\poker.exe --players AhKh QsQd --board 2c7d9h

# Options
.\build\poker.exe --players AsAd KhKc QsJs --trials 200000 --seed 7
.\build\poker.exe --players AhAs KdKc --json      # machine-readable output
```

Cards are `<rank><suit>`: rank in `2 3 4 5 6 7 8 9 T J Q K A`, suit in
`s h d c` (e.g. `As`, `Td`, `9c`). Flags: `--players`, `--board`, `--trials`
(default 100000), `--seed` (default 1), `--mc` (also run Monte Carlo post-flop),
`--json`, `--help`.

## Web UI

A browser front end (dark "analysis table" layout) is served by a small local
C++ HTTP server that links the same engine.

```powershell
.\build\poker_server.exe                 # then open http://127.0.0.1:8090
.\build\poker_server.exe --port 9000 --web web
```

Open **http://127.0.0.1:8090** (use `127.0.0.1`, not `localhost`, to avoid an
IPv6 fallback delay). Click a card slot to pick cards from a four-color grid;
each seat shows **Equity / Win / Tie** with an equity bar and a leader glow, and
hovering a seat reveals its full **hand-category distribution** (plus the current
made hand once a board is dealt). Preflop, a panel compares Monte Carlo against
full enumeration. Stop the server with Ctrl+C (or `Get-Process poker_server |
Stop-Process`).

Files: `server.cpp` + `http_server.h` (server), `web/index.html`,
`web/styles.css`, `web/app.js` (front end). The API is `GET /api/equity` (see
below) — the page just renders its JSON.

## JSON output

`--json` emits one object with a `results` array (an `enumeration` result, plus a
`monte_carlo` result preflop). Each player carries `win`, `tie`, `equity`,
`equity_ci`, and a `categories` map of probabilities. Intended as the data
contract for a future frontend (e.g. hover-to-see category odds).

## Verification

- `test_evaluator` — 23 checks across every hand category, the wheel straight,
  straight-flush > flush, full-house-from-two-trips, kicker tiebreaks, exact ties.
- `oracle_verify` — enumerates AA vs KK and AK vs QQ with a **separate**
  evaluator; matches the engine to the decimal (AA vs KK = 81.26% equity over all
  1,712,304 boards).
- Monte Carlo estimates land within their reported 95% CI of the exact values.
