export const BUILTIN_PACK = "fantasy";
export const ROLES = ["script", "soldier", "sword", "shield", "sprite", "banner", "strike", "horn", "crowd"];
const MAX_ARCHIVE = 32 * 1024 * 1024;
const MAX_FILE = 8 * 1024 * 1024;
const MAX_EXPANDED = 96 * 1024 * 1024;
const decoder = new TextDecoder("utf-8", { fatal: true });

export function packPath(value) {
  return typeof value === "string" && value.length > 0 && value.length < 160 &&
    value.split("/").every((part) => /^[a-z0-9_-][a-z0-9_.-]*$/.test(part));
}

export function readManifest(bytes, expected) {
  if (bytes.byteLength > 32768) throw new Error("The pack manifest is too large.");
  const manifest = JSON.parse(decoder.decode(bytes));
  if (manifest?.api !== 1 || !/^[a-z0-9_-]{1,32}$/.test(manifest?.id ?? "") ||
      (expected && manifest.id !== expected) ||
      typeof manifest.title !== "string" || !manifest.title.trim() ||
      new TextEncoder().encode(manifest.title).length >= 48 ||
      ROLES.some((role) => !packPath(manifest.files?.[role]))) {
    throw new Error("Invalid pack.json. Check the API version, ID and asset paths.");
  }
  return manifest;
}

function crc32(bytes) {
  let result = 0xffffffff;
  for (const byte of bytes) {
    result ^= byte;
    for (let bit = 0; bit < 8; bit += 1) {
      result = (result >>> 1) ^ ((result & 1) ? 0xedb88320 : 0);
    }
  }
  return (result ^ 0xffffffff) >>> 0;
}

async function inflate(blob, length) {
  const reader = new Blob([blob]).stream().pipeThrough(new DecompressionStream("deflate-raw")).getReader();
  const output = new Uint8Array(length);
  let offset = 0;
  try {
    for (;;) {
      const { done, value } = await reader.read();
      if (done) break;
      if (value.length > length - offset) throw new Error("ZIP entry exceeded its declared size.");
      output.set(value, offset);
      offset += value.length;
    }
  } finally {
    await reader.cancel().catch(() => {});
  }
  if (offset !== length) throw new Error("Incomplete ZIP entry.");
  return output;
}

export async function openZip(file) {
  if (file.size < 22 || file.size > MAX_ARCHIVE) throw new Error("Choose a ZIP pack up to 32 MB.");
  const bytes = new Uint8Array(await file.arrayBuffer());
  const view = new DataView(bytes.buffer);
  const u16 = (at) => view.getUint16(at, true);
  const u32 = (at) => view.getUint32(at, true);
  let end = bytes.length - 22;
  const earliest = Math.max(0, end - 65535);
  while (end >= earliest &&
         (u32(end) !== 0x06054b50 || end + 22 + u16(end + 20) !== bytes.length)) end -= 1;
  if (end < earliest) throw new Error("The ZIP directory could not be found.");
  const count = u16(end + 10);
  let entry = u32(end + 16);
  const start = entry;
  if (u16(end + 4) || u16(end + 6) || u16(end + 8) !== count ||
      count === 0 || count > 256 || start + u32(end + 12) !== end) {
    throw new Error("The ZIP directory is unsupported.");
  }

  const files = new Map();
  const paths = new Set();
  let total = 0;
  for (let index = 0; index < count; index += 1) {
    if (entry + 46 > end || u32(entry) !== 0x02014b50) throw new Error("Invalid ZIP directory entry.");
    const flags = u16(entry + 8);
    const method = u16(entry + 10);
    const checksum = u32(entry + 16);
    const compressed = u32(entry + 20);
    const size = u32(entry + 24);
    const nameSize = u16(entry + 28);
    const next = entry + 46 + nameSize + u16(entry + 30) + u16(entry + 32);
    const externalType = (u32(entry + 38) >>> 16) & 0xf000;
    const local = u32(entry + 42);
    if (next > end || local + 30 > start || u32(local) !== 0x04034b50 ||
        u16(local + 8) !== method || (flags & 1) || (u16(local + 6) & 1) || externalType === 0xa000 ||
        (method !== 0 && method !== 8)) {
      throw new Error("Unsupported or corrupt ZIP entry.");
    }

    const name = decoder.decode(bytes.subarray(entry + 46, entry + 46 + nameSize));
    const directory = name.endsWith("/");
    const path = directory ? name.slice(0, -1) : name;
    const dataAt = local + 30 + u16(local + 26) + u16(local + 28);
    if (!packPath(path) || paths.has(path) || u16(local + 26) !== nameSize ||
        dataAt + compressed > start ||
        decoder.decode(bytes.subarray(local + 30, local + 30 + nameSize)) !== name) {
      throw new Error("Unsafe, duplicate or inconsistent path in ZIP.");
    }
    paths.add(path);
    entry = next;
    if (directory) continue;
    total += size;
    if (size > MAX_FILE || total > MAX_EXPANDED) throw new Error("The unpacked content exceeds the pack limits.");
    const stored = bytes.subarray(dataAt, dataAt + compressed);
    const data = method === 0 ? stored : await inflate(stored, size);
    if (data.length !== size || crc32(data) !== checksum) throw new Error("A ZIP file failed its integrity check.");
    files.set(path, data);
  }
  if (entry !== end || !files.has("pack.json")) throw new Error("Put pack.json at the root of the ZIP.");
  const manifest = readManifest(files.get("pack.json"));
  if (manifest.id === BUILTIN_PACK) throw new Error("Give custom packs a new ID. The fantasy ID is reserved.");
  for (const path of Object.values(manifest.files)) {
    if (!files.has(path)) throw new Error(`The pack is missing ${path}.`);
  }
  return { manifest, files };
}

export async function fetchPack(fetcher) {
  const data = async (name) => {
    const response = await fetcher(name);
    if (!response.ok) throw new Error(`Could not load ${name}.`);
    return new Uint8Array(await response.arrayBuffer());
  };
  const root = `packs/${BUILTIN_PACK}/`;
  const manifestFile = await data(root + "pack.json");
  const manifest = readManifest(manifestFile, BUILTIN_PACK);
  const paths = [...new Set(ROLES.map((role) => manifest.files[role]))];
  const files = new Map(await Promise.all(paths.map(async (path) => [root + path, await data(root + path)])));
  files.set(root + "pack.json", manifestFile);
  for (const path of ["ui/font.txt", "shaders/world.vert", "shaders/world.frag"]) {
    files.set(path, await data(path));
  }
  return { manifest, files };
}

export function stageFiles(fs, root, files) {
  for (const [path, bytes] of files) {
    if (!packPath(path)) throw new Error("Invalid stored pack path.");
    const target = `${root}/${path}`;
    fs.mkdirTree(target.slice(0, target.lastIndexOf("/")));
    fs.writeFile(target, bytes);
  }
}

export function removeFiles(fs, directory) {
  for (const name of fs.readdir(directory)) {
    if (name === "." || name === "..") continue;
    const path = `${directory}/${name}`;
    if (fs.isDir(fs.stat(path).mode)) removeFiles(fs, path);
    else fs.unlink(path);
  }
  fs.rmdir(directory);
}
