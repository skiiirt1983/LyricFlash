/*
 * Local-file USB installer for LyricFlash Alpha/Beta.
 * File stays in the browser. ESP Web Tools reads a temporary blob: URL.
 * Hardware flashing still requires the user to authorize Web Serial.
 */
(() => {
  "use strict";

  const input = document.getElementById("localBin");
  const status = document.getElementById("localFileStatus");
  const install = document.getElementById("local-install");
  const note = document.getElementById("localInstallNote");
  if (!input || !status || !install || !note) return;

  let generation = 0;
  const expectedProject = "lyricflash";
  /* Existing-device updates must NEVER write a merged full image at 0x0.
     Extract only the validated application partition from that image. */
  const APP_OFFSET = 0x110000;
  const APP_SIZE = 0x500000;
  const OTA_DATA_OFFSET = 0x109000;
  const OTA_DATA_SIZE = 0x2000;

  const report = (message, bad = false) => {
    status.textContent = message;
    status.classList.toggle("error", bad);
  };

  const readChunk = async (file, offset, bytes) =>
    new Uint8Array(await file.slice(offset, offset + bytes).arrayBuffer());

  const partInfo = (data) => {
    const result = {};
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    for (let at = 0; at + 32 <= data.length; at += 32) {
      if (data[at] !== 0xAA || data[at + 1] !== 0x50) break;
      const label = Array.from(data.subarray(at + 12, at + 28))
        .map(n => n ? String.fromCharCode(n) : "")
        .join("");
      result[label] = {
        offset: view.getUint32(at + 4, true),
        size: view.getUint32(at + 8, true),
      };
    }
    return result;
  };

  const valid = async (file) => {
    if (!file.name.toLowerCase().endsWith(".bin")) throw Error("Select a .bin firmware image.");
    if (file.size < 0x110120 || file.size > 16 * 1024 * 1024)
      throw Error("This is not a complete ESP32-S3 USB image (expected merged firmware, not the smaller OTA image).");

    const [boot, table, app] = await Promise.all([
      readChunk(file, 0, 32),
      readChunk(file, 0x8000, 192),
      readChunk(file, 0x110000, 0x100),
    ]);

    if (boot[0] !== 0xE9 || boot[12] !== 9 || boot[13] !== 0)
      throw Error("Missing the ESP32-S3 bootloader at address 0x0.");
    const parts = partInfo(table);
    if (!parts.nvs || parts.nvs.offset !== 0x9000 || parts.nvs.size !== 0x100000 ||
        !parts.ota_0 || parts.ota_0.offset !== 0x110000 || parts.ota_0.size !== 0x500000 ||
        !parts.ota_1 || parts.ota_1.offset !== 0x610000 || parts.ota_1.size !== 0x500000)
      throw Error("The partition table does not match the expected 16 MB LyricFlash layout.");

    if (file.size - APP_OFFSET > APP_SIZE)
      throw Error("App section exceeds its 5 MB partition. Stop; do not erase flash.");
    if (app[0] !== 0xE9 || app[12] !== 9 || app[13] !== 0 ||
        app[0x20] !== 0x32 || app[0x21] !== 0x54 ||
        app[0x22] !== 0xCD || app[0x23] !== 0xAB)
      throw Error("Missing an ESP32-S3 application at 0x110000.");

    const project = new TextDecoder("ascii").decode(app.subarray(0x50, 0x70))
      .split("\0", 1)[0];
    if (project !== expectedProject)
      throw Error("Selected firmware is not LyricFlash. Choose lyricflash-full.bin, not an OTA or another manufacturer's image.");
    return true;
  };

  input.addEventListener("change", async () => {
    const request = ++generation;
    install.classList.add("hidden");
    note.classList.add("hidden");
    const file = input.files && input.files[0];
    if (!file) {
      report("Choose a complete USB firmware .bin to enable installation.");
      return;
    }
    report("Checking selected firmware...");
    try {
      await valid(file);
      await Promise.race([
        customElements.whenDefined("esp-web-install-button"),
        new Promise((_, reject) => setTimeout(
          () => reject(Error("USB installer unavailable. Check the network or try ESPHome Web.")),
          12000
        )),
      ]);
      if (request !== generation) return;

      /* The local source is a full image, but the actual installer writes
         ONLY the application + OTA-selection metadata. It never overwrites
         bootloader, partition table, NVS, or the other application slot. */
      const appImage = file.slice(APP_OFFSET);
      const appURL = URL.createObjectURL(appImage);
      const otaResetURL = URL.createObjectURL(new Blob([
        new Uint8Array(OTA_DATA_SIZE).fill(0xff),
      ], { type: "application/octet-stream" }));
      const manifest = {
        name: "LyricFlash EXISTING DEVICE — KEEP SETTINGS",
        version: "local-preserve-settings",
        new_install_prompt_erase: true,
        builds: [{
          chipFamily: "ESP32-S3",
          improv: false,
          parts: [
            { path: appURL, offset: APP_OFFSET },
            { path: otaResetURL, offset: OTA_DATA_OFFSET },
          ],
        }],
      };
      const manifestURL = URL.createObjectURL(new Blob(
        [JSON.stringify(manifest)], { type: "application/json" }
      ));
      install.manifest = manifestURL;
      install.classList.remove("hidden");
      note.classList.remove("hidden");
      report("Application verified in " + file.name + ". This installer writes only the app and OTA selector. KEEP DATA if prompted; cancel if you cannot keep settings.");
    } catch (error) {
      if (request === generation) {
        report(error.message || "Firmware file could not be checked.", true);
      }
    }
  });
})();
