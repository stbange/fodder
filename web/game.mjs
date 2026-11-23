import { ACTION, createInput } from "./input.mjs";
import { BUILTIN_PACK, fetchPack, openZip, removeFiles, stageFiles } from "./packs.mjs";
import { openStorage } from "./storage.mjs";
import { xrController } from "./xr.mjs";

export function mountFodder({
  window, document,
  loadEngine = async (options) => (await import("./fodder.mjs")).default(options),
  loadAssets = fetchPack, loadStorage = openStorage, makeXR = xrController, makeInput = createInput
}) {
  const $ = (id) => document.getElementById(id);
  const canvas = $("canvas");
  const dialog = $("pack-dialog");
  const menu = $("overlay");
  const deploy = $("deploy");
  const continueButton = $("continue");
  let mod, saves, xr, input;
  let visibleScreen = -1;
  let lastUi = "";
  let activePack = { id: BUILTIN_PACK, root: "/content/packs/fantasy" };
  let slot = 0;
  let importing = false;
  const touchControls = $("touch");
  let wasLocked = false;
  let lastFrame;
  let graphicsReady = true;

  function command(action) {
    if (!mod || importing || dialog.open) return;
    const before = mod._fodder_stat(0);
    mod._fodder_command(action);
    const after = mod._fodder_stat(0);
    if (before !== after) input.release();
    if (before !== 1 && after === 1 && !input.touch && !xr.active) {
      canvas.focus({ preventScroll: true });
      try { Promise.resolve(canvas.requestPointerLock?.()).catch(() => {}); }
      catch { /* Mouse dragging remains available. */ }
    }
    if (after !== 1 && document.pointerLockElement) document.exitPointerLock();
    renderUi();
  }

  function setTouch(enabled) {
    touchControls.hidden = !enabled || visibleScreen !== 1 || Boolean(xr?.active);
    $("quick-controls").textContent = enabled
      ? "Left pad Move · Right side Look · Strike and Block"
      : "WASD Move · Mouse Look · Left Strike · Right Block";
    $("control-help").textContent = enabled
      ? "Move with the left pad and drag on the right to look. Hold Strike or Block. Tap the pause button to take a break."
      : "WASD to move. Mouse to look. Left click to strike and right click to block. Escape to pause.";
    if (enabled && document.pointerLockElement) document.exitPointerLock();
  }

  function label(type, index = 0) {
    return mod.UTF8ToString(mod._fodder_label(type, index));
  }

  function renderUi() {
    const screen = mod._fodder_stat(0);
    const save = mod._fodder_stat(9);
    const selected = mod._fodder_stat(16);
    const key = [screen, save, selected, activePack.id, label(1), Number(xr.active)].join("|");
    if (key === lastUi) return;
    lastUi = key;
    visibleScreen = screen;
    $("game").dataset.screen = ["camp", "fight", "paused", "reward", "result"][screen];
    const fighting = screen === 1 && !save;
    menu.hidden = fighting || xr.active;
    $("battle-overlay").hidden = !fighting || xr.active;
    touchControls.hidden = !input.touch || !fighting || xr.active;
    $("camp").hidden = screen !== 0 || Boolean(save);
    $("armies").hidden = screen !== 0 || activePack.id !== BUILTIN_PACK;
    $("interlude").hidden = $("camp").hidden === false;
    $("packs").disabled = !saves || screen === 1 || screen === 2 || screen === 3 || Boolean(save);
    if (!fighting && document.pointerLockElement) document.exitPointerLock();
    if (fighting) return;

    $("arena").textContent = label(0);
    $("variant").textContent = label(1);
    $("hint").textContent = label(2);
    const kills = mod._fodder_stat(5), streak = mod._fodder_stat(7);
    $("profile").hidden = kills + streak === 0;
    $("best-kills").textContent = `Best kills ${kills}`;
    $("best-streak").textContent = `Best survival streak ${streak}`;

    const rewards = $("rewards");
    rewards.hidden = true;
    continueButton.hidden = false;
    continueButton.disabled = save === 1;
    const winner = mod._fodder_stat(13);
    const reason = ["", "The armies broke formation", "The final horn sounded", "You fell in combat"][mod._fodder_stat(14)];
    if (save) {
      $("result-title").textContent = save === 1 ? "Saving progression" : "Save failed";
      $("result-tag").textContent = label(1);
      $("result-description").textContent = save === 1 ? "Recording this battle." : "Retry the save or reload to recover your last saved progress.";
      continueButton.textContent = "Retry save";
    } else if (screen === 2) {
      $("result-title").textContent = "Battle paused";
      $("result-tag").textContent = label(1);
      $("result-description").textContent = "";
      continueButton.textContent = "Resume battle";
    } else if (screen === 3) {
      $("result-title").textContent = "You survived";
      $("result-tag").textContent = reason || "Survivor reward";
      const outcome = winner === 1 ? " The Reed Guard won." : winner === 2 ? " The Cinder Host won." : "";
      $("result-description").textContent = `${mod._fodder_stat(8)} enemy kills.${outcome} Choose a permanent upgrade.`;
      rewards.hidden = false;
      continueButton.hidden = true;
      rewards.replaceChildren();
      for (let i = 0; i < 3; i += 1) {
        const button = document.createElement("button");
        button.className = "reward";
        button.type = "button";
        button.setAttribute("aria-current", String(selected === i));
        const name = document.createElement("strong");
        const description = document.createElement("span");
        name.textContent = `${label(3, i)} · Rank ${mod._fodder_stat(10 + i) + 1}`;
        description.textContent = label(4, i);
        button.append(name, description);
        button.onclick = () => command(ACTION.first + i);
        rewards.append(button);
      }
    } else {
      $("result-title").textContent = "Fallen in battle";
      $("result-tag").textContent = label(1);
      $("result-description").textContent = `${mod._fodder_stat(8)} enemy kills. You keep every earned upgrade.`;
      continueButton.textContent = "Redeploy";
    }
  }

  function frame(timestamp) {
    window.requestAnimationFrame(frame);
    if (xr.active || !graphicsReady) {
      lastFrame = undefined;
      return;
    }
    const dt = lastFrame === undefined ? 0 : (timestamp - lastFrame) / 1000;
    lastFrame = timestamp;
    const bounds = canvas.getBoundingClientRect();
    const scale = Math.min(window.devicePixelRatio || 1, 450 / Math.max(1, bounds.height), 960 / Math.max(1, bounds.width));
    const width = Math.max(1, Math.round(bounds.width * scale)), height = Math.max(1, Math.round(bounds.height * scale));
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width;
      canvas.height = height;
    }
    mod._fodder_control(...input.sample());
    mod._fodder_flat(dt, width, height);
    renderUi();
  }

  async function showPacks() {
    $("pack-status").textContent = "";
    await listPacks();
    dialog.showModal();
  }

  async function listPacks() {
    const list = $("pack-list");
    list.replaceChildren();
    for (const pack of [{ manifest: { id: BUILTIN_PACK, title: "The Broken Ford" } }, ...await saves.allPacks()]) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "pack-button";
      button.textContent = pack.manifest.title + (pack.manifest.id === activePack.id ? " · Active" : "");
      button.onclick = () => runPackChange(() => activate(pack));
      list.append(button);
    }
  }

  async function activate(pack, install = false) {
    const previous = activePack;
    const id = pack.manifest.id;
    const root = id === BUILTIN_PACK ? "/content/packs/fantasy" : `/mods/pack-${++slot}`;
    const load = async (location, key) => {
      const profile = await saves.profile(key);
      if (!mod.ccall("fodder_load_pack", "number", ["string", "string", "string"], [location, key, profile])) {
        throw new Error(mod.UTF8ToString(mod._fodder_error()));
      }
    };
    try {
      if (pack.files) stageFiles(mod.FS, root, pack.files);
      await load(root, id);
      if (install) {
        try { await saves.putPack(pack); }
        catch (error) {
          await load(previous.root, previous.id);
          throw error;
        }
      }
    } catch (error) {
      if (pack.files && mod.FS.analyzePath(root).exists) removeFiles(mod.FS, root);
      throw error;
    }
    activePack = { id, root };
    if (previous.id !== BUILTIN_PACK) removeFiles(mod.FS, previous.root);
    input.release();
    dialog.close();
    lastUi = "";
    renderUi();
  }

  async function runPackChange(operation) {
    if (importing) return;
    importing = true;
    $("pack-file").disabled = true;
    $("pack-status").textContent = "Loading pack…";
    try { await operation(); }
    catch (error) { $("pack-status").textContent = error.message; }
    finally {
      importing = false;
      $("pack-file").disabled = false;
      $("pack-file").value = "";
    }
  }

  async function start() {
    const pack = await loadAssets((path) => window.fetch(new URL(`./assets/${path}`, import.meta.url)));
    saves = await loadStorage(window.indexedDB);
    const game = await loadEngine({ canvas, locateFile: (name) => new URL(name, import.meta.url).href });
    stageFiles(game.FS, "/content", pack.files);
    game.persistProfile = (id, text, revision) => {
      saves.saveProfile(id, text, revision).then(
        () => game._fodder_save_done(1),
        (error) => {
          console.warn(error.message);
          game._fodder_save_done(0);
        }
      );
    };
    const profile = await saves.profile(BUILTIN_PACK);
    if (!game.ccall("fodder_boot", "number", ["string"], [profile])) {
      throw new Error(game.UTF8ToString(game._fodder_error()));
    }
    mod = game;

    xr = makeXR({
      xr: window.navigator.xr, canvas, game, Framebuffer: window.XRWebGLLayer,
      onStart() {
        input.release();
        if (document.pointerLockElement) document.exitPointerLock();
        renderUi();
      },
      onEnd() {
        input.release();
        if (mod._fodder_stat(0) === 1) mod._fodder_command(ACTION.back);
        renderUi();
      },
      onError(error) { $("status").textContent = error.message; },
      onFrame: renderUi, isReady: () => graphicsReady
    });
    input = makeInput({
      window, document, canvas, moveZone: $("move-zone"), knob: $("knob"), lookZone: $("look-zone"),
      attack: $("attack"), block: $("block"), onAction: command, onTouch: setTouch
    });
    deploy.disabled = false;
    deploy.textContent = "Deploy";
    deploy.onclick = () => command(ACTION.confirm);
    continueButton.onclick = () => command(ACTION.confirm);
    $("pause").onclick = () => command(ACTION.back);
    $("xr-enter").onclick = () => xr.enter();
    $("packs").onclick = () => showPacks().catch((error) => { $("status").textContent = error.message; });
    $("input-mode").onchange = (event) => input.setMode(event.target.value);
    $("pack-file").onchange = (event) => {
      const file = event.target.files[0];
      if (file) runPackChange(async () => activate(await openZip(file), true));
    };
    canvas.onpointerdown = (event) => {
      if (event.pointerType === "mouse" && !input.touch && mod._fodder_stat(0) === 1 && !document.pointerLockElement) {
        try { Promise.resolve(canvas.requestPointerLock?.()).catch(() => {}); }
        catch { /* Mouse dragging remains available. */ }
      }
    };
    canvas.addEventListener("webglcontextlost", (event) => {
      event.preventDefault();
      graphicsReady = false;
      input.release();
    });
    canvas.addEventListener("webglcontextrestored", () => {
      graphicsReady = Boolean(mod._fodder_restore_graphics());
      if (!graphicsReady) {
        $("status").textContent = mod.UTF8ToString(mod._fodder_error());
        if (mod._fodder_stat(0) === 1) command(ACTION.back);
      }
    });
    document.addEventListener("pointerlockchange", () => {
      const locked = document.pointerLockElement === canvas;
      if (wasLocked && !locked && !input.touch && !xr.active && mod._fodder_stat(0) === 1) command(ACTION.back);
      wasLocked = locked;
    });
    window.addEventListener("blur", () => {
      if (!xr.active && mod._fodder_stat(0) === 1) command(ACTION.back);
    });
    xr.supported().then((supported) => { $("xr-enter").hidden = !supported; });
    renderUi();
    window.requestAnimationFrame(frame);
  }

  return start().catch((error) => {
    deploy.textContent = "Unable to load";
    $("status").textContent = error.message;
    $("packs").disabled = true;
  });
}

if (typeof window !== "undefined") mountFodder({ window, document });
