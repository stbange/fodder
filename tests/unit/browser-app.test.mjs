import { test } from "node:test";
import { equal, ok, match, deepEqual } from "node:assert/strict";
import { setImmediate, setTimeout } from "node:timers/promises";
import { mountFodder } from "../../web/game.mjs";
import { quickPack, zip } from "../zip-fixture.mjs";

class Element {
  listeners = new Map();
  dataset = {};
  children = [];
  attributes = {};
  textContent = "";
  added = 0;
  hidden = false;
  open = false;
  addEventListener(type, listener) { this.listeners.set(type, listener); }
  emit(type, event = {}) { this.listeners.get(type)?.(event); }
  getBoundingClientRect() { return { width: 1120, height: 720 }; }
  focus() { this.focused = true; }
  showModal() { this.open = true; }
  close() { this.open = false; }
  append(...children) { this.children.push(...children); }
  replaceChildren(...children) { this.children = children; }
  setAttribute(key, value) { this.attributes[key] = value; }
}

function fixture(options = {}) {
  const doc = new Element(), win = new Element();
  const nodes = new Map();
  doc.getElementById = (name) => {
    if (!nodes.has(name)) nodes.set(name, new Element());
    return nodes.get(name);
  };
  doc.createElement = () => new Element();
  doc.exitPointerLock = () => { doc.pointerLockElement = null; };
  const el = doc.getElementById, canvas = el("canvas");
  canvas.requestPointerLock = () => {
    if (options.lockThrows) throw new Error("Lock unavailable");
    if (options.lockReject) return Promise.reject(new Error("Lock declined"));
    doc.pointerLockElement = canvas;
    return Promise.resolve();
  };
  const frames = [];
  win.requestAnimationFrame = (callback) => frames.push(callback);
  win.navigator = {};
  win.devicePixelRatio = 1;
  const table = Array(19).fill(0);
  const writes = [], commands = [], restored = [], renders = [], loaded = [];
  let progress = "";
  let customTitle = "The Broken Ford";
  let mode = false, xrOptions, inputOptions;
  const packList = [...(options.initialPacks ?? [])];
  const mod = {
    _fodder_stat: (index) => table[index],
    _fodder_label: (name, i) => [customTitle, customTitle === "The Broken Ford" ? "Hold the Ford" : "Changed battle",
      "Battle hint", ["Wind", "Buckle", "Dressing"][i], "Upgrade description"][name],
    _fodder_command(action) {
      commands.push(action);
      if (table[0] === 3 && action >= 4) {
        table[0] = 0;
        table[10 + (action - 4)] += table[18] || 1;
        table[18] = 0;
      } else if (action === 0 && [0, 2, 4].includes(table[0])) {
        table[0] = 1;
      } else if (action === 1 && table[0] === 1) {
        table[0] = 2;
      }
    },
    _fodder_control: () => {},
    _fodder_flat: (...args) => renders.push(args),
    _fodder_drop_graphics: () => {},
    _fodder_restore_graphics: () => { restored.push(1); return !options.restoreFailed; },
    _fodder_error: () => "The game could not load that pack",
    _fodder_save_done: (valid) => writes.push(valid),
    UTF8ToString: (value) => value,
    ccall(command, _kind, _types, values) {
      if (command === "fodder_boot") return !options.bootFailed;
      const [root, id, text] = values;
      loaded.push([root, id, text]);
      if (options.reject === id) return false;
      customTitle = id === "fantasy" ? "The Broken Ford" : "Imported game";
      return true;
    },
    FS: {
      get files() { return files; },
      mkdirTree: (path) => directories.add(path),
      writeFile: (path, bytes) => files.set(path, bytes),
      analyzePath: (path) => ({ exists: [...files.keys()].some((entry) => entry.startsWith(path + "/")) }),
      readdir: (path) => [".", "..", ...new Set([...files.keys()].filter((entry) =>
        entry.startsWith(path + "/")).map((entry) => entry.slice(path.length + 1).split("/")[0]))],
      stat: (path) => ({ mode: files.has(path) ? "file" : "dir" }),
      isDir: (mode) => mode === "dir",
      unlink: (path) => files.delete(path),
      rmdir: (path) => directories.delete(path)
    }
  };
  const directories = new Set(), files = new Map();
  const storage = {
    profile: async () => progress,
    saveProfile: async (_id, text) => {
      if (options.saveFailed) throw new Error("Disk full");
      progress = text;
    },
    allPacks: async () => {
      if (options.listFailed) throw new Error("List unavailable");
      return packList;
    },
    putPack: async (pack) => {
      if (options.storeFailed) throw new Error("Storage full");
      packList.push(pack);
    }
  };
  const xr = { active: false, supported: async () => true, enter: () => {
    xr.active = true;
    xrOptions.onStart();
  }};
  const ready = mountFodder({
    window: win, document: doc,
    loadAssets: async () => ({ files: new Map([["pack.json", new Uint8Array()]]) }),
    loadEngine: async ({ locateFile }) => { match(locateFile("fodder.wasm"), /fodder\.wasm$/); return mod; },
    loadStorage: async () => storage,
    makeInput(args) {
      inputOptions = args;
      args.onTouch(false);
      return { get touch() { return mode; }, release: () => {},
        sample: () => [0, 0, 0, 0, 0, 0], setMode: (value) => { mode = value === "touch"; args.onTouch(mode); } };
    },
    makeXR(args) { xrOptions = args; return xr; }
  });

  return {
    ready, el, doc, win, mod, commands, writes, restored, renders, loaded, packList, xr,
    setStat(index, value) { table[index] = value; },
    send(action) { inputOptions.onAction(action); },
    frame(timestamp = 13) { frames.shift()?.(timestamp); },
    xrError() { xrOptions.onError(new Error("XR unavailable")); },
    exitXR() { xr.active = false; xrOptions.onEnd(); },
    submit(data) { el("pack-file").onchange({ target: { files: data ? [new Blob([data])] : [] } }); }
  };
}

async function eventually(test) {
  for (let i = 0; i < 50; i += 1) {
    if (test()) return;
    await setTimeout(10);
  }
  ok(test());
}

test("browser shell binds menus, falls back from pointer lock and restores graphics", async () => {
  const f = fixture({ lockThrows: true });
  await f.ready;
  const { el, doc } = f;
  equal(el("deploy").textContent, "Deploy");
  equal(el("variant").textContent, "Hold the Ford");
  f.frame(0);
  f.frame(16);
  deepEqual(f.renders.map((frame) => frame[0]), [0, .016]);
  el("deploy").onclick();
  equal(el("game").dataset.screen, "fight");
  ok(el("canvas").focused);
  el("canvas").onpointerdown({ pointerType: "mouse" });
  const canvas = el("canvas");
  doc.pointerLockElement = canvas;
  doc.emit("pointerlockchange");
  doc.pointerLockElement = null;
  doc.emit("pointerlockchange");
  equal(el("result-title").textContent, "Battle paused");
  el("continue").onclick();
  f.win.emit("blur");
  equal(el("game").dataset.screen, "paused");
  el("input-mode").onchange({ target: { value: "touch" } });
  el("continue").onclick();
  equal(el("touch").hidden, false);
  el("pause").onclick();
  equal(el("touch").hidden, true);
  el("xr-enter").onclick();
  equal(f.xr.active, true);
  f.frame(32);
  f.xrError();
  match(el("status").textContent, /XR unavailable/);
  f.exitXR();
  let prevented = false;
  canvas.emit("webglcontextlost", { preventDefault: () => { prevented = true; } });
  ok(prevented);
  f.frame(48);
  canvas.emit("webglcontextrestored");
  f.frame(64);
  equal(f.restored.length, 1);
});

test("browser shell renders saving, rewards, defeat, and personal records", async () => {
  const f = fixture({ lockReject: true, saveFailed: true, restoreFailed: true });
  await f.ready;
  const { el, mod } = f;
  el("deploy").onclick();
  el("canvas").onpointerdown({ pointerType: "mouse" });
  el("canvas").emit("webglcontextrestored");
  equal(el("result-title").textContent, "Battle paused");
  f.setStat(9, 1);
  f.frame();
  // Resume only once WebGL has been restored by the browser.
  equal(el("status").textContent, "The game could not load that pack");

  const ready = fixture({ saveFailed: true });
  await ready.ready;
  const n = ready.el;
  ready.setStat(0, 1);
  ready.setStat(9, 1);
  ready.frame();
  equal(n("result-title").textContent, "Saving progression");
  ok(n("continue").disabled);
  ready.setStat(9, 2);
  ready.frame();
  equal(n("result-title").textContent, "Save failed");
  ready.setStat(9, 0);
  ready.setStat(0, 3);
  ready.setStat(8, 12);
  ready.setStat(13, 2);
  ready.setStat(18, 2);
  ready.frame();
  match(n("result-description").textContent, /12 enemy kills.*Cinder Host won/);
  equal(n("rewards").children.length, 3);
  match(n("result-description").textContent, /extra rank/);
  match(n("rewards").children[0].children[0].textContent, /Rank 2/);
  n("rewards").children[0].onclick();
  equal(n("game").dataset.screen, "camp");
  equal(ready.mod._fodder_stat(10), 2);
  ready.setStat(0, 4);
  ready.setStat(5, 12);
  ready.setStat(7, 2);
  ready.frame();
  equal(n("result-title").textContent, "Fallen in battle");
  equal(n("best-kills").textContent, "Best kills 12");
  equal(n("best-streak").textContent, "Best survival streak 2");
  mod.persistProfile("fantasy", "invalid", 0);
  await setImmediate();
  deepEqual(f.writes, [0]);
  const normal = fixture();
  await normal.ready;
  normal.mod.persistProfile("fantasy", "saved", 0);
  await setImmediate();
  deepEqual(normal.writes, [1]);
});

test("browser shell installs packs and rolls back a failed save", async () => {
  const f = fixture({ reject: "bad-pack" });
  await f.ready;
  const n = f.el;
  await n("packs").onclick();
  ok(n("pack-dialog").open);
  n("pack-file").onchange({ target: { files: [] } });
  f.submit(zip(quickPack({ id: "bad-pack" })));
  await eventually(() => n("pack-status").textContent.includes("could not load"));
  f.submit(zip(quickPack()));
  f.send(0);
  await eventually(() => !n("pack-dialog").open);
  equal(f.packList.length, 1);
  equal(n("variant").textContent, "Changed battle");
  await n("packs").onclick();
  await n("pack-list").children[0].onclick();
  equal(n("variant").textContent, "Hold the Ford");
  ok(f.loaded.some(([, id]) => id === "quick-ford"));
  const rollback = fixture({ storeFailed: true });
  await rollback.ready;
  await rollback.el("packs").onclick();
  rollback.submit(zip(quickPack()));
  await eventually(() => rollback.el("pack-status").textContent === "Storage full");
  deepEqual(rollback.loaded.map((record) => record[1]), ["quick-ford", "fantasy"]);
  const error = fixture({ listFailed: true });
  await error.ready;
  await error.el("packs").onclick();
  equal(error.el("status").textContent, "List unavailable");
  const bootError = fixture({ bootFailed: true });
  await bootError.ready;
  equal(bootError.el("deploy").textContent, "Unable to load");
});
