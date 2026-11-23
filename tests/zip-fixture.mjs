import { deflateRawSync } from "node:zlib";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";

function crc32(bytes) {
  let value = -1;
  for (const byte of bytes) {
    value ^= byte;
    for (let bit = 0; bit < 8; bit += 1) value = (value >>> 1) ^ ((value & 1) ? 0xedb88320 : 0);
  }
  return (value ^ -1) >>> 0;
}

export function zip(entries, { method = 0, corrupt = false } = {}) {
  const files = [], directory = [];
  let offset = 0;
  for (const [filename, raw] of entries) {
    const name = Buffer.from(filename), body = Buffer.from(raw);
    const packed = method === 8 ? deflateRawSync(body) : body;
    const hash = crc32(body) ^ Number(corrupt);
    const local = Buffer.alloc(30), central = Buffer.alloc(46);
    local.writeUInt32LE(0x04034b50);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(method, 8);
    local.writeUInt32LE(hash >>> 0, 14);
    local.writeUInt32LE(packed.length, 18);
    local.writeUInt32LE(body.length, 22);
    local.writeUInt16LE(name.length, 26);
    central.writeUInt32LE(0x02014b50);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(method, 10);
    central.writeUInt32LE(hash >>> 0, 16);
    central.writeUInt32LE(packed.length, 20);
    central.writeUInt32LE(body.length, 24);
    central.writeUInt16LE(name.length, 28);
    central.writeUInt32LE(offset, 42);
    offset += local.length + name.length + packed.length;
    files.push(local, name, packed);
    directory.push(central, name);
  }
  const index = Buffer.concat(directory), end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(index.length, 12);
  end.writeUInt32LE(offset, 16);
  return Buffer.concat([...files, index, end]);
}

export function quickPack({ id = "quick-ford", title = "Quick Ford test pack", invalid = false } = {}) {
  const root = resolve("assets/packs/fantasy");
  const manifest = JSON.parse(readFileSync(resolve(root, "pack.json"), "utf8"));
  manifest.id = id;
  manifest.title = title;
  const entries = Object.values(manifest.files).map((path) => [path, readFileSync(resolve(root, path))]);
  entries[0][1] = invalid ? "this is invalid Lua" : entries[0][1].toString()
    .replaceAll(/seconds = seconds, rout = 0.29/g, "seconds = 2, rout = 0.29")
    .replaceAll("enemy_damage = 5.5 + hostile * 1.2", "enemy_damage = 0")
    .replace("Hold the Ford", "The quick crossing");
  return [["pack.json", JSON.stringify(manifest)], ...entries];
}
