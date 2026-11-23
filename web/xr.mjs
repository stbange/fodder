import { ACTION } from "./input.mjs";

const I = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];

export function multiply(a, b) {
  const result = new Float32Array(16);
  for (let col = 0; col < 4; col += 1) {
    for (let row = 0; row < 4; row += 1) {
      for (let i = 0; i < 4; i += 1) result[col * 4 + row] += a[i * 4 + row] * b[col * 4 + i];
    }
  }
  return result;
}

function shift(x, y, z) {
  const matrix = [...I];
  matrix[12] = x;
  matrix[13] = y;
  matrix[14] = z;
  return matrix;
}

function turn(angle) {
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  return [c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0, 0, 0, 0, 1];
}

export function eyeMatrices(view, head, origin, anchor, yaw) {
  const worldToReference = multiply(shift(...anchor), multiply(turn(-yaw), shift(...origin.map((p) => -p))));
  const referenceToWorld = multiply(shift(...origin), multiply(turn(yaw), shift(...anchor.map((p) => -p))));
  const viewInReference = view.transform.inverse.matrix;
  const eyeWorld = multiply(referenceToWorld, view.transform.matrix);
  return [
    ...multiply(view.projectionMatrix, multiply(viewInReference, worldToReference)),
    ...multiply(view.projectionMatrix, multiply(viewInReference, head)),
    eyeWorld[12], eyeWorld[13], eyeWorld[14], Math.atan2(-eyeWorld[8], eyeWorld[10])
  ];
}

export function axes(gamepad) {
  const values = gamepad?.axes ?? [];
  return values.length >= 4 ? [values[2], values[3]] : [values[0] ?? 0, values[1] ?? 0];
}

function pressed(gamepad, index) {
  return Boolean(gamepad?.buttons?.[index]?.pressed);
}

export function xrController({ xr, canvas, game, Framebuffer, onStart, onEnd, onError, onFrame = () => {}, isReady = () => true }) {
  let session = null;
  let requesting = false;
  async function supported() {
    try { return Boolean(await xr?.isSessionSupported("immersive-vr")); }
    catch { return false; }
  }

  async function enter() {
    if (session || requesting || !xr) return;
    requesting = true;
    let pending;
    try {
      pending = await xr.requestSession("immersive-vr", { optionalFeatures: ["local-floor"] });
      const gl = canvas.getContext("webgl2");
      if (!gl) throw new Error("The WebGL 2 context is unavailable.");
      await gl.makeXRCompatible();
      const layer = new Framebuffer(pending, gl, { alpha: false, depth: true, antialias: false, framebufferScaleFactor: 0.82 });
      pending.updateRenderState({ baseLayer: layer, depthNear: 0.045, depthFar: 260 });
      let floor = true;
      const reference = await pending.requestReferenceSpace("local-floor").catch(() => {
        floor = false;
        return pending.requestReferenceSpace("local");
      });
      session = pending;
      const active = pending;
      let anchor, previous, offset = 0, turnLatch = false, menuLatch = false, selectLatch = false;
      let pauseLatch = false;
      game.xrFramebuffer = layer.framebuffer;
      onStart();
      active.addEventListener("end", () => {
        session = null;
        game.xrFramebuffer = null;
        game._fodder_clear_input();
        onEnd();
      }, { once: true });

      function frame(time, xrFrame) {
        if (session !== active) return;
        active.requestAnimationFrame(frame);
        if (!isReady()) { previous = undefined; return; }
        const pose = xrFrame.getViewerPose(reference);
        if (!pose) return;
        const dt = previous === undefined ? 0 : (time - previous) / 1000;
        previous = time;
        const head = pose.transform.matrix;
        const heading = Math.atan2(-head[8], head[10]);
        if (!anchor) {
          anchor = [head[12], floor ? 0 : head[13] - 1.67, head[14]];
          offset = game._fodder_world(3) - heading;
        }
        const sources = [...active.inputSources];
        const left = sources.find((source) => source.handedness === "left")?.gamepad;
        const right = sources.find((source) => source.handedness === "right")?.gamepad;
        const [strafe, move] = axes(left);
        const [turnX] = axes(right);
        const select = pressed(right, 0) || pressed(left, 0);
        const pause = pressed(left, 4) || pressed(right, 4);
        let screen = game._fodder_stat(0);
        if (pause && !pauseLatch) game._fodder_command(ACTION.back);
        pauseLatch = pause;
        if (screen !== 1 && select && !selectLatch) game._fodder_command(ACTION.confirm);
        if (screen === 3 && Math.abs(move) > 0.6 && !menuLatch) {
          game._fodder_command(move > 0 ? ACTION.next : ACTION.previous);
        }
        menuLatch = Math.abs(move) > 0.6;
        if (screen === 1 && Math.abs(turnX) > 0.65 && !turnLatch) {
          offset += Math.sign(turnX) * Math.PI / 6;
        }
        turnLatch = Math.abs(turnX) > 0.48;
        screen = game._fodder_stat(0);
        game._fodder_control(
          Math.abs(move) > 0.16 ? -move : 0, Math.abs(strafe) > 0.16 ? strafe : 0, 0, 0,
          Number(screen === 1 && select && selectLatch), Number(pressed(left, 1) || pressed(right, 1))
        );
        selectLatch = select;
        game._fodder_xr_step(dt, heading + offset);
        const origin = [0, 1, 2].map((index) => game._fodder_world(index));
        const address = game._fodder_xr_buffer() / 4;
        for (const view of pose.views) {
          const viewport = layer.getViewport(view);
          game.HEAPF32.set(eyeMatrices(view, head, origin, anchor, offset), address);
          game._fodder_xr_view(viewport.x, viewport.y, viewport.width, viewport.height);
        }
        onFrame();
      }

      active.requestAnimationFrame(frame);
    } catch (error) {
      if (pending) await pending.end().catch(() => {});
      onError(error);
    } finally {
      requesting = false;
    }
  }
  return { supported, enter, get active() { return session !== null; } };
}
