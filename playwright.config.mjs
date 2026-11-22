import { defineConfig } from "@playwright/test";

const address = process.env.FODDER_WEB_BASE_URL ?? "http://127.0.0.1:4101/fodder/";
export default defineConfig({
  testDir: "./tests/browser",
  outputDir: "./tests/browser/results",
  workers: 1,
  timeout: 45000,
  use: {
    baseURL: address,
    viewport: { width: 1120, height: 720 },
    launchOptions: {
      executablePath: process.env.FODDER_CHROMIUM_EXECUTABLE || undefined,
      args: ["--enable-unsafe-swiftshader", "--use-gl=angle", "--use-angle=swiftshader"]
    },
    screenshot: "only-on-failure"
  },
  webServer: process.env.FODDER_WEB_BASE_URL ? undefined : {
    command: "node tools/serve.mjs build/web/play 4101",
    url: address,
    reuseExistingServer: !process.env.CI,
    timeout: 15000
  }
});
