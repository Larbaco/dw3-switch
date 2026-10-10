# Native Switch features

Phase 10 goal note. See [ROADMAP.md](ROADMAP.md) P10.1–P10.5.

Many “Switch-native” behaviours are already owned by earlier phases (present,
pad/rumble, saves, scaling, turbo). This note is the inventory and the home
for **residual** Switch-only UX after those phases land. Hosts stay thin:
`platforms/switch/` implements applet/hid/present; policy and toggles live in
`port/` and [PORT_OPTIONS.md](PORT_OPTIONS.md).

## Already owned elsewhere (do not re-spec in Phase 10)

| Behaviour | Roadmap | Layer |
|---|---|---|
| Docked / handheld present, 60 Hz vsync | P4.2 | `platforms/switch` (`nx_gpu`) |
| Hid → PS1 pad; `PadSetAct` → HD rumble | P4.4 | `nx_input` |
| `sdmc` / `romfs`, timing | P4.5 | `nx_file` / `nx_os` |
| `.mcr` files, import/export, flush on exit / focus loss, rotating backups, sleep-safe delta clamp | P5.3 (expanded) | `port/memcard` + Switch host loop |
| Crash / assert log | P5.4 | host |
| Integer scale, aspect, **widescreen** (wanted; PSY-Z PC present already fits by aspect) | P6.1 | present + port options |
| PAL/NTSC + EU `CNTY_SEL` / language | P6.2 | game + host pacing |
| Fast-forward / frame-skip | P6.4 | `port/` + host tick |
| Title-menu toggles + persistence | Phase 9 | `port/` |

## Phase 10 residual (Switch-focused)

| Item | Default | Notes |
|---|---|---|
| Focus loss: pause audio/logic; clamp time delta on resume | always on | Complements P5.3 flush; prevents battle/CD jumps after sleep |
| Confirm/Cancel layout (PS positional vs Nintendo A/B) | PS positional | Toggle in Phase 9 |
| System language → EU `LANGUAGE` hint | off / follow stock boot | Override still via `CNTY_SEL` / packs |
| `swkbd` for tamer name | off | Filter to game charset/font |
| CRT / sharp present filters | off | Present-stage only |
| Separate BGM / SFX volumes | 100/100 | Host mixer |
| Multi memcard files / user-profile save folder | off | Builds on P5.3 paths |
| Capture button / album screenshot | best-effort | Video capture likely N/A in homebrew |
| Amiibo, gyro aiming, touch-in-game, NSO cloud | skip | Poor fit for DW3 / retail-only |

## Layering

| Layer | Responsibility |
|---|---|
| `platforms/switch/` | Applet focus/operation mode, hid rumble, present modes, swkbd/error applets |
| `port/` | Option registry, save backup rotation policy, turbo/widescreen flags shared with PC |
| Owner SD layout | `.mcr`, backups, `port_options` — never in git |

## Homebrew caveats

- Prefer **applet mode** heap; do not require title takeover for the gate.
- Display is 60 Hz; PAL 50 Hz pacing is software (see P6.2).
- Lite has no rumble; ignore actuators when absent.
- Official cloud / play activity / eShop packaging stay out of scope
  ([ARCHITECTURE.md](ARCHITECTURE.md) non-goals).
