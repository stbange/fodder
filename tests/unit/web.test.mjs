import { test } from "node:test";
import { deepEqual, equal, fail, ok, rejects, throws } from "node:assert/strict";
import { readFileSync } from "node:fs";
import { IDBFactory } from "fake-indexeddb";
import { ACTION, createInput } from "../../web/input.mjs";
import { fetchPack, openZip, packPath, readManifest, removeFiles, stageFiles } from "../../web/packs.mjs";
import { openStorage } from "../../web/storage.mjs";
import { axes, eyeMatrices, multiply, xrController } from "../../web/xr.mjs";
import { quickPack, zip } from "../zip-fixture.mjs";

const packed = (items, options) => new Blob([zip(items, options)]);
const read = (path) => readFileSync(`assets/${path}`);

test("manifest paths and ZIP packing are portable and bounded", async () => {
  for (const path of ["a/b.lua", "folder-1/a_b.ogg", "pack.json"]) ok(packPath(path));
  for (const path of ["", ".env", "/x", "../x", "x/./z", "A.lua", "x//z", "x\\z", "a".repeat(160), 1]) {
    equal(packPath(path), false);
  }
  for (const method of [0, 8]) {
    const bytes = packed([["scripts/", ""], ...quickPack()], { method });
    const result = await openZip(bytes);
    equal(result.manifest.id, "quick-ford");
    equal(result.files.size, 10);
    ok(result.files.has("scripts/battles.lua"));
  }
  throws(() => readManifest(new Uint8Array(32769)), /too large/);
  throws(() => readManifest(read("packs/fantasy/pack.json"), "different"), /Invalid/);
  throws(() => readManifest(new TextEncoder().encode("{}")), /Invalid/);
  await rejects(openZip(new Blob(["bad"])), /32 MB/);
  await rejects(openZip(new Blob([new Uint8Array(22)])), /directory could not/);
  await rejects(openZip(packed([])), /unsupported/);
  await rejects(openZip(packed([["a.txt", "a"]], { method: 4 })), /Unsupported/);
  await rejects(openZip(packed([["../a.txt", "a"]])), /Unsafe/);
  await rejects(openZip(packed([["a.txt", "a"], ["a.txt", "a"]])), /duplicate/);
  await rejects(openZip(packed([["a.txt", "a"]], { corrupt: true })), /integrity/);
  await rejects(openZip(packed([["a.txt", "a"]])), /root/);
  await rejects(openZip(packed(quickPack({ id: "fantasy" }))), /reserved/);
  await rejects(openZip(packed(quickPack().slice(0, -1))), /missing/);
  await rejects(openZip(packed([["big.bin", new Uint8Array(8 * 1024 * 1024 + 1)]])), /limits/);
  const changed = zip([["a.txt", "a"]], { method: 8 });
  const start = changed.readUInt32LE(changed.length - 6);
  changed.writeUInt32LE(2, start + 24);
  await rejects(openZip(new Blob([changed])), /Incomplete/);
  changed.writeUInt32LE(0, start + 24);
  await rejects(openZip(new Blob([changed])), /exceeded/);
  changed.writeUInt32LE(0, start);
  await rejects(openZip(new Blob([changed])), /Invalid ZIP directory entry/);
});

test("built-in pack fetches loose resources and stages them in the VM", async () => {
  const received = [];
  const pack = await fetchPack(async (name) => {
    received.push(name);
    return new Response(read(name));
  });
  equal(received.length, 13);
  ok(received.includes("shaders/world.vert"));
  equal(pack.manifest.id, "fantasy");
  await rejects(fetchPack(async () => new Response("", { status: 404 })), /Could not load/);
  const written = new Map();
  const fs = {
    mkdirTree: (path) => ok(path.startsWith("/content")),
    writeFile: (path, bytes) => written.set(path, bytes),
    readdir: (root) => root === "/content" ? [".", "..", "child", "a"] : [".", "..", "b"],
    stat: (path) => ({ mode: path.endsWith("child") ? 1 : 0 }),
    isDir: (mode) => mode === 1,
    unlink: (path) => unlinked.push(path),
    rmdir: (path) => unlinked.push(path)
  };
  const unlinked = [];
  stageFiles(fs, "/content", pack.files);
  equal(written.size, pack.files.size);
  throws(() => stageFiles(fs, "/content", new Map([["..", "x"]])), /Invalid stored/);
  removeFiles(fs, "/content");
  deepEqual(unlinked, ["/content/child/b", "/content/child", "/content/a", "/content"]);
});

test("storage isolates profiles and protects against concurrent writes", async () => {
  const database = new IDBFactory();
  const storage = await openStorage(database);
  const second = await openStorage(database);
  equal(await storage.profile("fantasy"), "");
  await storage.saveProfile("fantasy", "first", 0);
  await rejects(second.saveProfile("fantasy", "other", 0), /Another tab/);
  await storage.saveProfile("fantasy", "second", 1);
  equal(await second.profile("fantasy"), "second");
  equal(await storage.profile("custom"), "");
  await storage.putPack({ manifest: { id: "custom", title: "Example" }, files: new Map() });
  deepEqual(await second.allPacks(), [{ manifest: { id: "custom", title: "Example" }, files: new Map() }]);
  for (const event of ["blocked", "error"]) {
    const request = { error: new Error("Database unavailable") };
    const opening = openStorage({ open: () => request });
    request[`on${event}`]();
    await rejects(opening, /Database unavailable|Close other/);
  }
});

class Target {
  listeners = new Map();
  style = {};
  captures = [];
  addEventListener(name, listener) { this.listeners.set(name, listener); }
  setPointerCapture(id) { this.captures.push(id); }
  getBoundingClientRect() { return { left: 0, top: 0, width: 100, height: 100 }; }
  emit(name, fields = {}) {
    let prevented = false;
    this.listeners.get(name)?.({ type: name, preventDefault: () => { prevented = true; }, ...fields });
    return prevented;
  }
}

test("input combines devices, latches taps, and releases cancelled pointers", () => {
  const window = new Target(), document = new Target(), canvas = new Target(), moveZone = new Target();
  const knob = new Target(), lookZone = new Target(), attack = new Target(), block = new Target();
  let coarse = false;
  window.matchMedia = () => ({ matches: coarse });
  const actions = [], touch = [];
  const input = createInput({ window, document, canvas, moveZone, knob, lookZone, attack, block,
    onAction: (action) => actions.push(action), onTouch: (active) => touch.push(active) });
  equal(input.touch, false);
  equal(window.emit("keydown", { code: "Enter" }), true);
  window.emit("keydown", { code: "Escape", repeat: true });
  window.emit("keydown", { code: "Digit2", ctrlKey: true });
  window.emit("keydown", { code: "Digit3", target: { matches: () => true } });
  window.emit("keydown", { code: "KeyW" });
  window.emit("keydown", { code: "KeyD" });
  window.emit("keyup", { code: "KeyD" });
  canvas.emit("contextmenu");
  window.emit("pointermove", { movementX: 100 });
  canvas.emit("pointerdown", { button: 0, pointerType: "mouse" });
  canvas.emit("pointerdown", { button: 2, pointerType: "mouse" });
  window.emit("pointerup", { button: 0 });
  window.emit("pointermove", { movementX: 10, movementY: -5 });
  const first = input.sample();
  deepEqual(first.slice(0, 2), [1, 0]);
  ok(first[2] > .02 && first[3] > .01 && first[4] && first[5]);
  window.emit("pointerup", { button: 2 });
  deepEqual(input.sample(), [1, 0, 0, 0, 0, 0]);
  deepEqual(actions, [ACTION.confirm]);

  canvas.emit("pointerdown", { pointerType: "touch" });
  canvas.emit("pointerdown", { pointerType: "mouse", button: 0 });
  ok(input.touch && touch.at(-1));
  moveZone.emit("pointerdown", { pointerId: 1, clientX: 75, clientY: 20 });
  moveZone.emit("pointerdown", { pointerId: 2, clientX: 0, clientY: 0 });
  moveZone.emit("pointermove", { pointerId: 8, clientX: 0, clientY: 0 });
  moveZone.emit("pointermove", { pointerId: 1, clientX: 66, clientY: 30 });
  lookZone.emit("pointerdown", { pointerId: 2, clientX: 0, clientY: 0 });
  lookZone.emit("pointerdown", { pointerId: 3 });
  lookZone.emit("pointermove", { pointerId: 99 });
  lookZone.emit("pointermove", { pointerId: 2, clientX: 10, clientY: 4 });
  attack.emit("pointerdown", { pointerId: 3 });
  attack.emit("pointerdown", { pointerId: 9 });
  block.emit("pointerdown", { pointerId: 4 });
  block.emit("pointerdown", { pointerId: 9 });
  const motion = input.sample();
  ok(motion[0] > 0 && motion[1] > 0 && motion[2] > 0 && motion[3] < 0 && motion[4] && motion[5]);
  moveZone.emit("pointercancel", { pointerId: 1 });
  lookZone.emit("lostpointercapture", { pointerId: 2 });
  attack.emit("pointercancel", { pointerId: 3 });
  block.emit("pointerup", { pointerId: 4 });
  deepEqual(input.sample(), [0, 0, 0, 0, 0, 0]);
  input.setMode("mouse");
  equal(input.touch, false);
  canvas.emit("pointerdown", { pointerType: "touch" });
  equal(input.touch, false);
  input.setMode("touch");
  equal(input.touch, true);
  coarse = true;
  input.setMode("auto");
  document.hidden = true;
  document.emit("visibilitychange");
  window.emit("blur");
  input.release();
  equal(knob.style.transform, "");
});

const identity = [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];

test("XR transforms preserve view-relative equipment and world positions", () => {
  deepEqual(Array.from(multiply(identity, identity)), identity);
  const pose = eyeMatrices({
    projectionMatrix: identity, transform: { matrix: identity, inverse: { matrix: identity } }
  }, identity, [5, 1, 7], [0, 0, 0], 0);
  equal(pose.length, 36);
  deepEqual(pose.slice(12, 15), [-5, -1, -7]);
  deepEqual(pose.slice(16, 32), identity);
  deepEqual(pose.slice(32, 35), [5, 1, 7]);
  deepEqual(axes({ axes: [4, 5, .2, .3] }), [.2, .3]);
  deepEqual(axes({ axes: [.3, -.2] }), [.3, -.2]);
  deepEqual(axes(), [0, 0]);
});

function mockVR({ floor = true, valid = true } = {}) {
  const views = [], actions = [], updates = [];
  const frames = [];
  const left = { buttons: [{}, {}, {}, {}, {}], axes: [0, 0, 0, 0] };
  const right = { buttons: [{}, {}, {}, {}, {}], axes: [0, 0, 0, 0] };
  let screen = 0, ended = 0, started = 0, error;
  const session = Object.assign(new EventTarget(), {
    inputSources: [{ handedness: "left", gamepad: left }, { handedness: "right", gamepad: right }],
    updateRenderState: (value) => ok(value.baseLayer),
    requestReferenceSpace: async (type) => {
      if (!floor && type === "local-floor") throw new Error("floor unsupported");
      return type;
    },
    requestAnimationFrame: (callback) => frames.push(callback),
    async end() { this.dispatchEvent(new Event("end")); }
  });
  const game = {
    HEAPF32: new Float32Array(36), _fodder_world: () => 0, _fodder_xr_buffer: () => 0,
    _fodder_clear_input: () => {},
    _fodder_control: (...input) => updates.push(input),
    _fodder_xr_step: (dt, angle) => ok(dt >= 0 && Number.isFinite(angle)),
    _fodder_xr_view: (...viewport) => views.push(viewport),
    _fodder_stat: () => screen,
    _fodder_command: (action) => { actions.push(action); if (action === ACTION.confirm) screen = 1; }
  };
  class Framebuffer {
    framebuffer = {};
    getViewport() { return { x: 0, y: 0, width: 1, height: 1 }; }
  }
  const api = xrController({
    xr: { isSessionSupported: async () => true, requestSession: async () => session },
    canvas: { getContext: () => valid ? { makeXRCompatible: async () => {} } : null },
    game, Framebuffer, onStart() { started += 1; }, onEnd() { ended += 1; }, onError(value) { error = value; }
  });
  const transform = { matrix: identity, inverse: { matrix: identity } };
  const pose = { transform, views: [{ transform, projectionMatrix: identity }, { transform, projectionMatrix: identity }] };
  let timestamp = 0;
  return { api, session, right, left, views, actions, updates,
    get ended() { return ended; }, get started() { return started; }, get error() { return error; },
    screen(value) { screen = value; },
    frame(tracked = true) { frames.shift()(timestamp += 17, { getViewerPose: () => tracked ? pose : null }); }
  };
}

test("XR sessions handle controller input, reference fallback and release", async () => {
  const noDevice = xrController({});
  equal(await noDevice.supported(), false);
  await noDevice.enter();
  const unsupported = xrController({ xr: { isSessionSupported: async () => { throw new Error(); } } });
  equal(await unsupported.supported(), false);
  const rig = mockVR({ floor: false });
  equal(await rig.api.supported(), true);
  await Promise.all([rig.api.enter(), rig.api.enter()]);
  ok(rig.api.active && rig.started === 1);
  rig.frame(false);
  rig.right.buttons[0].pressed = true;
  rig.frame();
  ok(rig.actions.includes(ACTION.confirm));
  rig.left.axes = [0,0,.5,-.8];
  rig.right.axes[2] = .9;
  rig.right.buttons[1].pressed = true;
  rig.frame();
  deepEqual(rig.updates.at(-1), [.8, .5, 0, 0, 1, 1]);
  rig.left.axes[3] = 0;
  rig.frame();
  rig.screen(3);
  rig.left.axes[3] = .9;
  rig.right.buttons[4].pressed = true;
  rig.frame();
  ok(rig.actions.includes(ACTION.back) && rig.actions.includes(ACTION.next));
  equal(rig.views.length, 8);
  await rig.session.end();
  rig.frame();
  ok(!rig.api.active && rig.ended === 1);
  const failed = mockVR({ valid: false });
  await failed.api.enter();
  equal(failed.api.active, false);
  ok(failed.error?.message.includes("unavailable"));
});
