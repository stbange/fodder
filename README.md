# Fodder

Deploy as one soldier in a thousand at the Broken Ford. Move with the Reed Guard,
cross the river, or fight through the ruins. Stay alive until an army routs or
the final horn sounds. Each survival earns a permanent upgrade rank. Earn two
deeds by landing counters or killing enemies to gain a second rank in the
upgrade you choose. Your kill count and survival streak are recorded alongside
your army's result.

## Play a native build

Download the package for your operating system from a successful GitHub Actions
run. Extract the platform archive inside it so `assets/` is beside the executable or
`Fodder.app`. Press **Enter** to deploy. Move with **WASD**, look with the mouse,
strike with the left button, and block with the right button. Press **Escape**
to pause. Click once per swing. Raise your shield just before an incoming
strike lands to stagger the attacker, then release block and strike during the
opening. You can also step clear of a committed swing. Watch the banks for
enemy flankers. Choose upgrades with **1–3** or
select one with the arrow keys and **Enter**. [Builds](https://github.com/stbange/fodder/actions)

The web build switches to touch controls on phones. Drag the pad on the left to
move and drag on the right to look. Use the buttons to strike, block, or pause.
You can pick a control mode in the browser's **Controls** menu.

On a supported headset browser, **Enter VR** opens an immersive session.
Use the left stick to move or select an upgrade, the right stick to snap turn,
the trigger to strike or confirm, grip to block, and the primary face button to
pause. [Meta Quest browser documentation](https://developers.meta.com/vr/documentation/web/)

## Build

Use CMake 3.24+, Ninja, and a compiler with C17 support. Windows can build from
a Visual Studio Developer Command Prompt. On Linux, install your distribution's
OpenGL, X11, and audio development packages if you want the windowed game.

```sh
cmake --preset native
cmake --build --preset native
ctest --preset native
```

The package is in `build/native/play/`. On macOS the executable is inside
`Fodder.app`. From another working directory you can launch the game by
absolute path and it will still find its loose assets.

Activate Emscripten 4.0.16 and use Node.js 24 for browser tooling.

```sh
cmake --preset web
cmake --build --preset web
npm ci
node tools/serve.mjs build/web/play
```

Open `http://localhost:4101/fodder/`. The package in `build/web/play/` is
static and uses project-relative paths. GitHub Actions deploys it from
`main` once the repository's Pages source is set to **GitHub Actions**.

## Edit a content pack

Copy `assets/packs/fantasy/` and choose a new directory and matching ID.
IDs use lowercase ASCII, numbers, hyphens, or underscores. Keep `pack.json`
in the pack's root with API version 1 and the paths to its Lua script, meshes,
sprites, textures, and audio. Asset paths use lowercase portable names such as
`models/levy.mesh`.

The main Lua script returns an arena, a list of battles, three named upgrades,
and an `effects` function for converting saved ranks to combat stats.
Edit the duration, morale break threshold, lane preference, damage, fog,
ruins, and upgrades there. The optional `flankers` count puts soldiers from the
enemy roster into each flanking detachment. The engine validates the descriptor
and limits Lua to built-in table, string, and math operations and the return
values it expects.
Use the fantasy pack as a starting point for another genre.

- Desktop players can run `fodder --pack your-pack-id` with the new folder in
  `assets/packs/`. Use `FODDER_ASSETS` to select another asset tree.
- Browser players can ZIP a pack with `pack.json` at the ZIP root and choose
  **Content packs → Import ZIP**. Import supports store and deflate entries.
  Imported packs and their independent profiles persist in browser storage.
- Models use the text `FODDERM1` format with a vertex count and one line per
  vertex containing `x y z r g b team_tint`. Each group of three vertices makes
  a triangle. `FODDERM2` adds a final part number for soldier animation. Use
  `0` for body, `1` for the sword arm, and `2` for the shield arm. The sword
  arm pivots about `(0.32, 1.16, -0.06)` in model space. Sprites and banners use
  uncompressed, bottom-origin 32-bit TGA and sounds use WAV. Regenerate assets with
  `python3 tools/generate_art.py`.

## Verify

```sh
npm run spec
npm run coverage:js
cmake --preset coverage
cmake --build --preset coverage
xvfb-run -a ctest --preset coverage
npm exec -- playwright install chromium
npm run test:browser
```

The GitHub workflow enforces 95% executable-line unit coverage for C built on
Linux, Lua gameplay scripts, and browser JavaScript. The platform component
tests use OpenGL through Xvfb. Headless battle scenarios and Playwright tests
run as separate checks. Native smoke tests exercise the
SDL/OpenGL launch on Linux. The browser tests exercise mobile touch and a
mock immersive session. Physical phone and Quest 3 play and performance are
tracked as release checks in OpenSpec. [OpenSpec changes](openspec/changes/)

## Licenses

First-party code, including Lua and GLSL, is MIT. Original models, sprites,
textures, icon, font, and audio are CC BY 4.0. Packages also include the
third-party notices. [MIT](LICENSE) · [Art credits](assets/ATTRIBUTION.md)
