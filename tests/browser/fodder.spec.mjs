import { expect, test, devices } from "@playwright/test";
import { quickPack, zip } from "../zip-fixture.mjs";

async function camp(page) {
  await page.goto("./");
  await expect(page.getByRole("button", { name: "Deploy", exact: true })).toBeEnabled();
}

function upload(page, buffer) {
  return page.locator("#pack-file").setInputFiles({ name: "fantasy-test.zip", mimeType: "application/zip", buffer });
}

test("renders the WebAssembly game and deploys", async ({ page }) => {
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  const wasm = page.waitForResponse((response) => response.url().endsWith("/fodder.wasm"));
  await camp(page);
  expect((await wasm).headers()["content-type"]).toContain("application/wasm");
  await expect(page.getByRole("heading", { name: "Hold the Ford" })).toBeVisible();
  await page.getByRole("button", { name: "Deploy", exact: true }).click();
  await expect(page.locator("#game")).toHaveAttribute("data-screen", "fight");
  await page.keyboard.press("Escape");
  await expect(page.getByRole("heading", { name: "Battle paused" })).toBeVisible();
  await page.getByRole("button", { name: "Resume battle" }).click();
  await expect(page.locator("#game")).toHaveAttribute("data-screen", "fight");
  expect(errors).toEqual([]);
});

test("winds up, sweeps the first-person sword, and returns to guard", async ({ page }) => {
  await page.addInitScript(() => {
    window.swordPositions = [];
    const models = new WeakSet();
    const prototype = WebGL2RenderingContext.prototype;
    const getUniform = prototype.getUniformLocation;
    const setMatrix = prototype.uniformMatrix4fv;
    prototype.getUniformLocation = function (program, name) {
      const location = getUniform.call(this, program, name);
      if (location && name === "uModel") models.add(location);
      return location;
    };
    prototype.uniformMatrix4fv = function (location, transpose, matrix, offset = 0) {
      if (models.has(location)) {
        const [x, y, z] = matrix.subarray(offset + 12, offset + 15);
        if (y > -0.45 && z < -0.65 && z > -1.05) window.swordPositions.push(x);
      }
      return setMatrix.apply(this, arguments);
    };
  });
  await camp(page);
  await page.getByRole("button", { name: "Deploy", exact: true }).click();
  const positions = () => page.evaluate(() => window.swordPositions);
  await expect.poll(async () => Math.abs((await positions()).at(-1) - 0.43)).toBeLessThan(0.01);
  await page.mouse.move(560, 360);
  await page.mouse.down();
  await expect.poll(async () => Math.max(...await positions())).toBeGreaterThan(0.57);
  await expect.poll(async () => Math.min(...await positions())).toBeLessThan(-0.15);
  await page.mouse.up();
  await expect.poll(async () => Math.abs((await positions()).at(-1) - 0.43)).toBeLessThan(0.01);
});

test("selects mobile controls from real Chromium touch events", async ({ browser }) => {
  const context = await browser.newContext({ ...devices["iPhone 13"], viewport: { width: 844, height: 390 } });
  const page = await context.newPage();
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  await page.goto(new URL("./", test.info().project.use.baseURL).href);
  await page.getByRole("button", { name: "Deploy", exact: true }).tap();
  await expect(page.locator("#touch")).toBeVisible();
  const pad = await page.locator("#move-zone").boundingBox();
  const attack = await page.locator("#attack").boundingBox();
  const cdp = await context.newCDPSession(page);
  const point = { x: Math.round(pad.x + pad.width * .5), y: Math.round(pad.y + pad.height * .25), id: 1 };
  const strike = { x: Math.round(attack.x + attack.width * .5), y: Math.round(attack.y + attack.height * .5), id: 2 };
  await cdp.send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [point] });
  await expect(page.locator("#knob")).toHaveAttribute("style", /translate/);
  await cdp.send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [point, strike] });
  await cdp.send("Input.dispatchTouchEvent", { type: "touchCancel", touchPoints: [] });
  await expect(page.locator("#knob")).toHaveAttribute("style", "");
  await page.getByRole("button", { name: "Pause battle" }).tap();
  await expect(page.getByRole("heading", { name: "Battle paused" })).toBeVisible();
  expect(errors).toEqual([]);
  await context.close();
});

test("installs a ZIP, restores a pending reward, and saves its upgrade", async ({ page }) => {
  await camp(page);
  await page.getByRole("button", { name: "Content packs" }).click();
  await upload(page, zip(quickPack({ invalid: true }), { method: 8 }));
  await expect(page.locator("#pack-status")).toContainText("Lua:");
  await upload(page, zip(quickPack(), { method: 8 }));
  await expect(page.getByRole("heading", { name: "The quick crossing" })).toBeVisible();
  await page.getByRole("button", { name: "Deploy", exact: true }).click();
  await expect(page.getByRole("heading", { name: "You survived" })).toBeVisible({ timeout: 18000 });
  await page.reload();
  await expect(page.getByRole("button", { name: "Deploy", exact: true })).toBeEnabled();
  await expect(page.getByRole("heading", { name: "Hold the Ford" })).toBeVisible();
  await page.getByRole("button", { name: "Content packs" }).click();
  await page.getByRole("button", { name: "Quick Ford test pack" }).click();
  await expect(page.getByRole("heading", { name: "You survived" })).toBeVisible();
  await page.getByRole("button", { name: /Field Dressing · Rank 1/ }).click();
  await expect(page.getByRole("heading", { name: "Fog on the Water" })).toBeVisible();
  await page.getByRole("button", { name: "Deploy", exact: true }).click();
  await expect(page.getByRole("button", { name: /Field Dressing · Rank 2/ })).toBeVisible({ timeout: 18000 });
});

test("plays and claims a reward across two WebXR views, then returns to flat play", async ({ page }) => {
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  await page.addInitScript(() => {
    const identity = Float32Array.of(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1);
    const head = identity.slice(), inverse = identity.slice();
    head[13] = 1.67;
    inverse[13] = -1.67;
    const projection = Float32Array.of(1,0,0,0,0,1,0,0,0,0,-1,-1,0,0,-.09,0);
    const transform = { matrix: head, inverse: { matrix: inverse } };
    const pose = { transform, views: ["left","right"].map((eye) => ({
      eye, transform, projectionMatrix: projection
    }))};
    const buttons = Array.from({ length: 5 }, () => ({ pressed: false }));
    const xrSession = Object.assign(new EventTarget(), {
      inputSources: [{ handedness: "right", gamepad: { buttons, axes: [0,0,0,0] } }],
      requestReferenceSpace: async () => ({}),
      updateRenderState: () => {},
      requestAnimationFrame(callback) {
        window.requestAnimationFrame((time) => callback(time, { getViewerPose: () => pose }));
      },
      async end() { this.dispatchEvent(new Event("end")); }
    });
    window.fodderTestXR = { views: 0, buttons, xrSession };
    Object.defineProperty(navigator, "xr", { value: {
      isSessionSupported: async () => true, requestSession: async () => xrSession
    }});
    WebGL2RenderingContext.prototype.makeXRCompatible = async () => {};
    Object.defineProperty(window, "XRWebGLLayer", { value: class {
      constructor(session, gl) {
        this.framebuffer = gl.createFramebuffer();
        gl.bindFramebuffer(gl.FRAMEBUFFER, this.framebuffer);
        const texture = gl.createTexture();
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, 960, 540, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
        gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, texture, 0);
        const depth = gl.createRenderbuffer();
        gl.bindRenderbuffer(gl.RENDERBUFFER, depth);
        gl.renderbufferStorage(gl.RENDERBUFFER, gl.DEPTH_COMPONENT16, 960, 540);
        gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.DEPTH_ATTACHMENT, gl.RENDERBUFFER, depth);
      }
      getViewport(view) {
        window.fodderTestXR.views += 1;
        return { x: view.eye === "left" ? 0 : 480, y: 0, width: 480, height: 540 };
      }
    }});
  });
  await camp(page);
  await page.getByRole("button", { name: "Content packs" }).click();
  await upload(page, zip(quickPack({ id: "xr-ford" })));
  await expect(page.getByRole("heading", { name: "The quick crossing" })).toBeVisible();
  await page.getByRole("button", { name: "Enter VR" }).click();
  const views = () => page.evaluate(() => window.fodderTestXR.views);
  const trigger = (pressed) => page.evaluate((pressed) => { window.fodderTestXR.buttons[0].pressed = pressed; }, pressed);
  await expect.poll(views).toBeGreaterThan(2);
  await trigger(true);
  await expect(page.locator("#game")).toHaveAttribute("data-screen", "fight");
  await trigger(false);
  await expect(page.locator("#game")).toHaveAttribute("data-screen", "reward", { timeout: 18000 });
  await trigger(true);
  await expect(page.locator("#game")).toHaveAttribute("data-screen", "camp");
  await trigger(false);
  const frame = await views();
  await expect.poll(views).toBeGreaterThan(frame + 1);
  await trigger(true);
  await expect(page.locator("#game")).toHaveAttribute("data-screen", "fight");
  await page.evaluate(() => window.fodderTestXR.xrSession.end());
  await expect(page.getByRole("button", { name: "Resume battle" })).toBeVisible();
  expect(errors).toEqual([]);
});

test("resumes rendering after a WebGL context restore", async ({ page }) => {
  await page.addInitScript(() => {
    window.fodderDraws = 0;
    const prototype = WebGL2RenderingContext.prototype;
    const draw = prototype.drawArraysInstanced;
    prototype.drawArraysInstanced = function (...args) {
      window.fodderDraws += 1;
      return draw.apply(this, args);
    };
  });
  const invalid = [];
  page.on("console", (message) => {
    if (message.text().includes("object does not belong")) invalid.push(message.text());
  });
  await camp(page);
  const recovered = await page.evaluate(() => new Promise((resolve) => {
    const canvas = document.querySelector("canvas");
    const gl = canvas.getContext("webgl2");
    const extension = gl.getExtension("WEBGL_lose_context");
    if (!extension) return resolve(false);
    const timer = setTimeout(() => resolve(false), 6000);
    canvas.addEventListener("webglcontextlost", () => {
      setTimeout(() => extension.restoreContext(), 150);
    }, { once: true });
    canvas.addEventListener("webglcontextrestored", () => {
      clearTimeout(timer);
      resolve(!gl.isContextLost());
    }, { once: true });
    extension.loseContext();
  }));
  expect(recovered).toBe(true);
  const draws = await page.evaluate(() => window.fodderDraws);
  await page.getByRole("button", { name: "Deploy", exact: true }).click();
  await expect(page.locator("#game")).toHaveAttribute("data-screen", "fight");
  await expect.poll(() => page.evaluate(() => window.fodderDraws)).toBeGreaterThan(draws + 4);
  expect(invalid).toEqual([]);
});
