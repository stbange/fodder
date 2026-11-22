export const ACTION = Object.freeze({ confirm: 0, back: 1, previous: 2, next: 3, first: 4 });
const shortcuts = new Map([
  ["Enter", ACTION.confirm], ["Space", ACTION.confirm], ["Escape", ACTION.back],
  ["ArrowUp", ACTION.previous], ["ArrowLeft", ACTION.previous],
  ["ArrowDown", ACTION.next], ["ArrowRight", ACTION.next],
  ["Digit1", ACTION.first], ["Digit2", ACTION.first + 1], ["Digit3", ACTION.first + 2]
]);

export function createInput({ window, document, canvas, moveZone, knob, lookZone, attack, block, onAction, onTouch }) {
  const keys = new Set();
  let mouseAttack = false, mouseBlock = false, touchAttack = null, touchBlock = null, tap = false;
  let move = null, look = null, forward = 0, side = 0, lookX = 0, lookY = 0;
  let mode = "auto";
  let usingTouch = window.matchMedia("(pointer: coarse)").matches;
  const capture = (node, event) => node.setPointerCapture?.(event.pointerId);
  const hold = (event) => event.preventDefault();
  function release() {
    keys.clear();
    mouseAttack = mouseBlock = tap = false;
    move = look = touchAttack = touchBlock = null;
    forward = side = lookX = lookY = 0;
    knob.style.transform = "";
  }
  function touchMode(enabled) {
    if (enabled !== usingTouch) release();
    usingTouch = enabled;
    onTouch(enabled);
  }
  function setMode(next) {
    mode = next;
    touchMode(next === "touch" || (next === "auto" && window.matchMedia("(pointer: coarse)").matches));
  }
  function updatePad(event) {
    const box = moveZone.getBoundingClientRect();
    const radius = Math.max(14, Math.min(box.width, box.height) * 0.43);
    const x = event.clientX - box.left - box.width / 2;
    const y = event.clientY - box.top - box.height / 2;
    const denominator = Math.max(radius, Math.hypot(x, y));
    side = x / denominator;
    forward = -y / denominator;
    knob.style.transform = `translate(${side * radius}px, ${-forward * radius}px)`;
  }

  const onKey = (event, down) => {
    if (event.target?.matches?.("button,select,input") || event.metaKey || event.ctrlKey || event.altKey) return;
    if (down) keys.add(event.code);
    else keys.delete(event.code);
    const action = shortcuts.get(event.code);
    if (down && action !== undefined && !event.repeat) {
      hold(event);
      onAction(action);
    }
  };
  window.addEventListener("keydown", (event) => onKey(event, true));
  window.addEventListener("keyup", (event) => onKey(event, false));
  window.addEventListener("blur", release);
  document.addEventListener("visibilitychange", () => { if (document.hidden) release(); });
  canvas.addEventListener("contextmenu", hold);
  canvas.addEventListener("pointerdown", (event) => {
    if (event.pointerType === "touch") {
      if (mode === "auto") touchMode(true);
      return;
    }
    if (usingTouch) return;
    if (event.button === 0) mouseAttack = tap = true;
    if (event.button === 2) mouseBlock = true;
  });
  window.addEventListener("pointerup", (event) => {
    if (event.button === 0) mouseAttack = false;
    if (event.button === 2) mouseBlock = false;
  });
  window.addEventListener("pointermove", (event) => {
    if (usingTouch || (document.pointerLockElement !== canvas && !mouseAttack && !mouseBlock)) return;
    lookX += event.movementX * 0.00235;
    lookY -= event.movementY * 0.00235;
  });
  moveZone.addEventListener("pointerdown", (event) => {
    if (move !== null) return;
    move = event.pointerId;
    capture(moveZone, event);
    updatePad(event);
  });
  moveZone.addEventListener("pointermove", (event) => {
    if (event.pointerId === move) updatePad(event);
  });
  lookZone.addEventListener("pointerdown", (event) => {
    if (look !== null) return;
    look = { id: event.pointerId, x: event.clientX, y: event.clientY };
    capture(lookZone, event);
  });
  lookZone.addEventListener("pointermove", (event) => {
    if (look?.id !== event.pointerId) return;
    lookX += (event.clientX - look.x) * 0.0054;
    lookY -= (event.clientY - look.y) * 0.0054;
    look.x = event.clientX;
    look.y = event.clientY;
  });
  attack.addEventListener("pointerdown", (event) => {
    if (touchAttack !== null) return;
    touchAttack = event.pointerId;
    tap = true;
    capture(attack, event);
  });
  block.addEventListener("pointerdown", (event) => {
    if (touchBlock !== null) return;
    touchBlock = event.pointerId;
    capture(block, event);
  });
  const endTouch = (event) => {
    const id = event.pointerId;
    if (id === move) {
      move = null;
      forward = side = 0;
      knob.style.transform = "";
    }
    if (id === look?.id) look = null;
    if (id === touchAttack) {
      touchAttack = null;
      if (event.type === "pointercancel") tap = false;
    }
    if (id === touchBlock) touchBlock = null;
  };
  for (const node of [moveZone, lookZone, attack, block]) {
    node.addEventListener("pointerup", endTouch);
    node.addEventListener("pointercancel", endTouch);
    node.addEventListener("lostpointercapture", endTouch);
  }
  onTouch(usingTouch);

  return {
    release, setMode,
    get touch() { return usingTouch; },
    sample() {
      const result = [
        forward + Number(keys.has("KeyW")) - Number(keys.has("KeyS")),
        side + Number(keys.has("KeyD")) - Number(keys.has("KeyA")),
        lookX, lookY, Number(mouseAttack || touchAttack !== null || tap), Number(mouseBlock || touchBlock !== null)
      ];
      lookX = lookY = 0;
      tap = false;
      return result;
    }
  };
}
