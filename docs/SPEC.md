# Snapline behaviour spec

Clean-room behaviour spec for a Lightshot-style screenshot tool on Linux. It records *what* the reference app does, worked out from static inspection (strings, resources, installer layout). No code, icons, text or branding from the reference app go into Snapline.

**Reference examined:** Lightshot 5.5.0.7 for Windows.
- Installer SHA-256: `928c2808421dfd487ffa697379548cbe682c0e13aeb595eb89973ba9c515b8a1`
- `Lightshot.dll` SHA-256: `04a27f0d1e89341722461119e00a10e00ec2a52f5e305961161ec4378e610e93`

Status tags: **[observed]** = confirmed in the binaries or string tables. **[assumed]** = public/common knowledge that hasn't been checked against the binary yet (verify under Wine or by decompiling).

## 1. Architecture of the reference (for orientation only)
| Component | Role |
|---|---|
| `Lightshot.exe` (launcher) | Starts the versioned app folder |
| `5.5.0.7/Lightshot.exe` | Tray app, options dialog (Hotkeys / Proxy / General pages), hotkey registration |
| `Lightshot.dll` | Capture overlay, editor tools, save/copy/print |
| `DXGIODScreenshot.dll` | Fast desktop capture (DXGI) |
| `uploader.dll`, `net.dll` | Upload, share URLs, proxy handling |
| `locales/*.txt` | UI strings, `[[key]]=[[value]]` format, ~50 languages |

The installer also bundles a third-party browser offer (Yandex). **Snapline ships no bundled offers and no telemetry** (the reference contains Google Analytics calls).

## 2. Entry points
- **Main hotkey** opens the capture overlay. Default `Print` [assumed]. The tray tooltip says "Press the %hotkey% on your keyboard to take a screenshot" [observed].
- **Instant save fullscreen** hotkey: optional and can be turned on/off. Saves the whole screen with no overlay [observed]. Default `Shift+Print` [assumed].
- **Instant upload fullscreen** hotkey: optional and can be turned on/off [observed]. Off by default [assumed].
- **Tray left-click** = take a screenshot. **Tray menu:** Take a screenshot, (account items, out of scope), Options, Help, About, Exit [observed].
- **Single instance:** a second launch forwards its command to the running instance (the reference uses a named mutex) [observed].
- **Hotkey registration fails:** show a notification "Failed to register a hotkey: <key>. Likely another application is already using it." Clicking it opens Options [observed].

## 3. Capture overlay
- Freezes the current screen content and dims it across all monitors.
- The tooltip near the cursor reads "Select area" [observed].
- Drag to select a rectangle. While dragging and after, a live size label shows `WxH` [observed: `%dx%d`].
- After selecting, the rectangle can be moved and resized with handles [assumed].
- Clicking outside it starts a new selection [assumed].
- **Keep selected area position** option: the next capture starts with the previous rectangle [observed].
- **Capture cursor** option: draws the mouse pointer into the image [observed].
- "Wrong size" error if the selection is empty or too small to act on [observed].
- Context/extra actions: Select full screen, Clear selection, Cancel, Close [observed].

### 3.1 Editor toolbar (shown beside the selection)
Tools [observed]: **Pen, Line, Arrow, Rectangle, Circle, Marker, Text, Blur, Color, Undo**.
- Marker = semi-transparent wide highlighter stroke.
- Blur = pixelate/blur the dragged rectangle.
- Color opens a colour picker and applies to the next shapes.
- Undo removes the last annotation.
- Snapline adds **Redo** (Ctrl+Shift+Z) and a **stroke-width** control (mouse wheel), as improvements.

### 3.2 Action toolbar (shown below/next to the selection)
[observed]
- **Upload** (Snapline: "Upload" to the backend the user has configured)
- **Share** menu: needs a public URL, so it uploads first and then opens:
  - Google: search similar images
  - TinEye: search similar images
  - Send via email (`mailto:` / `xdg-email`)
  - Twitter/X, Facebook, VK, Pinterest share URLs
- **Edit online:** out of scope for v1.
- **Print**
- **Copy**
- **Save**
- **Close**

### 3.3 Keyboard shortcuts inside the overlay
[assumed, verify]

| Key | Action |
|---|---|
| Ctrl+C | Copy |
| Ctrl+S | Save |
| Ctrl+D | Upload |
| Ctrl+P | Print |
| Ctrl+Z | Undo |
| Ctrl+A | Select full screen |
| Esc | Cancel |
| Ctrl+Shift+Z | Redo (Snapline addition) |
| Arrow keys | Nudge the selection by 1 px (Snapline addition) |
| Shift+Arrow keys | Nudge the selection by 10 px (Snapline addition) |

## 4. Post-capture behaviour and notifications
[observed]
| Action | Result | Notification text (Snapline wording) |
|---|---|---|
| Copy | Image goes to the clipboard, overlay closes | "Screenshot copied to clipboard" |
| Save | Save dialog, default name `Screenshot_N` (`.png` / `.jpg`) | "Saved to <file>. Click to open the folder." |
| Upload | Progress window "Uploading image", then the link | "Uploaded. Link copied to clipboard." (if AutoCopy) |
| Upload fails | Dialog offering: open app proxy settings / open system proxy settings / retry / cancel, plus detailed info | |
| Bad server response | "Incorrect response from the image hosting server" | |

Notifications only show when **Show notifications** is on.

## 5. Settings
Stored in `~/.config/snapline/snapline.conf`. Reference names are given for mapping only; they are not reused.

| Snapline key | Reference equivalent | Type / default |
|---|---|---|
| `hotkeys/main` | Hotkey_main_* | key sequence, `Print`, on |
| `hotkeys/save_full` | Hotkey_savefull_* | key sequence, `Shift+Print`, on |
| `hotkeys/upload_full` | Hotkey_uploadfull_* | key sequence, empty, off |
| `general/auto_copy_link` | AutoCopy | bool, true |
| `general/auto_close_upload_window` | AutoClose | bool, true |
| `general/show_notifications` | ShowBubbles | bool, true |
| `general/capture_cursor` | CaptureCursor | bool, false |
| `general/keep_selection` | KeepSelection | bool, false |
| `general/language` | Language | system |
| `format/upload_format` | UploadFormat | `png` \| `jpg`, `png` |
| `format/jpeg_quality` | JpegQuality | 1–100, 90 [assumed] |
| `format/optimize` | (OptiPNG option) | `never` \| `always` \| `upload_only`; runs `optipng` if installed |
| `save/directory` | (last used dir) | `~/Pictures/Screenshots` |
| `save/filename_template` | `Screenshot_%d` | `Screenshot_%Y-%m-%d_%H-%M-%S` |
| `network/proxy_type` | ProxyType | `none` \| `system` \| `manual`, `system` |
| `network/proxy` | ProxyString | `host:port` |
| `upload/backend` | (fixed: prntscr) | `imgur` \| `custom`, `imgur` |
| `upload/imgur_client_id` | — | string |
| `upload/custom/*` | — | url, method, file field, headers, link JSON path |

**Hotkey validation** [observed]:
- "One of your hotkeys is invalid": a hotkey that is turned on but empty or unparseable.
- "You can not set the same hotkeys for different functions": two hotkeys are the same.

## 6. Out of scope / replaced
- prntscr.com upload/API and the account sign-in/gallery are replaced by pluggable backends plus a local upload **history** (tray menu: last N links, click to copy).
- Edit online: not in v1.
- Telemetry and bundled offers: never.

## 7. Linux-specific requirements (no reference equivalent)
- **X11:** grab the screen directly; global hotkeys via XCB key grab.
- **Wayland:** capture via xdg-desktop-portal Screenshot; `grim` fallback on wlroots compositors. Global hotkeys via the portal GlobalShortcuts, with a fallback to a desktop-environment custom shortcut that runs `snapline gui`.
- Clipboard ownership survives the overlay closing (the tray process stays alive).
- HiDPI and mixed-DPI multi-monitor setups must produce pixel-accurate crops.
