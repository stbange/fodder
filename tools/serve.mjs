import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { resolve, extname, sep } from "node:path";

const root = resolve(process.argv[2] ?? "build/web/play");
const port = Number(process.argv[3] ?? "4101");
const types = { ".html": "text/html", ".css": "text/css", ".mjs": "text/javascript",
  ".wasm": "application/wasm", ".json": "application/json", ".png": "image/png", ".md": "text/plain" };

createServer(async (request, response) => {
  try {
    const url = new URL(request.url, "http://localhost");
    if (!url.pathname.startsWith("/fodder/")) {
      response.writeHead(404).end("Open /fodder/");
      return;
    }
    const file = resolve(root, decodeURIComponent(url.pathname.slice(8)) || "index.html");
    if (!file.startsWith(root + sep)) {
      response.writeHead(403).end();
      return;
    }
    const content = await readFile(file);
    response.writeHead(200, { "Content-Type": types[extname(file)] ?? "application/octet-stream" }).end(content);
  } catch {
    response.writeHead(404).end();
  }
}).listen(port, "0.0.0.0", () => console.log(`Fodder preview on http://localhost:${port}/fodder/`));
