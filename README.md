# Snapline

Region screenshots for Linux: press a hotkey, drag over what you want, mark it up, then copy, save, print or upload it.

Snapline is an independent, open-source project. It is not affiliated with Lightshot or Skillbrains.

## Features

- **Select** an area on any monitor. A live `W×H` label shows the size. Move/resize with the handles or nudge with the arrow keys (Shift = 10 px).
- **Annotate** with pen, line, arrow, rectangle, circle, marker, text and blur. Change the colour from the palette and the stroke size with the mouse wheel. Hold Shift for 45° lines, squares and circles. Undo/redo.
- **Output:** copy, save, print, or upload to Imgur or your own HTTP endpoint. After uploading, the link goes to your clipboard and into the *Recent uploads* tray menu.
- **Share** an uploaded image: reverse image search (Google Lens, TinEye), email, X, Facebook, VK, Pinterest.
- **Instant hotkeys:** save the full screen, or upload the full screen.
- X11 and Wayland (GNOME, KDE Plasma, Sway, Hyprland and other wlroots compositors), multi-monitor, HiDPI and mixed DPI.

### Keyboard shortcuts in the capture overlay

| Key | Action |
|---|---|
| Ctrl+C / Enter | Copy |
| Ctrl+S | Save |
| Ctrl+D | Upload |
| Ctrl+P | Print |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo |
| Ctrl+A | Select the whole monitor |
| Arrow keys | Move the selection |
| Esc / right-click | Cancel |

## Install

| Distribution | Package |
|---|---|
| Debian, Ubuntu, Mint, Pop!_OS | `.deb` from Releases (PPA planned) |
| Fedora, openSUSE | `.rpm` from Releases (COPR/OBS planned) |
| Arch, Manjaro, EndeavourOS | `PKGBUILD` in `packaging/arch` (AUR planned) |
| Anything else | AppImage or Flatpak from Releases |

## Usage

Start `snapline`; it sits in the tray. The default hotkeys:

- `Print`: select an area
- `Shift+Print`: save the full screen

Commands, also usable from your desktop's shortcut settings:

```
snapline gui           # select an area
snapline full          # save the full screen now
snapline full-upload   # upload the full screen now
snapline settings
snapline quit
```

### Wayland notes

- **GNOME and KDE:** screen capture goes through the desktop portal. Global hotkeys go through the GlobalShortcuts portal, and your desktop may ask you to confirm them the first time. GNOME binds `Print` to its own screenshot tool. *Settings → Hotkeys → Add the general hotkey as a GNOME shortcut* can hand `Print` to Snapline.
- **Sway, Hyprland and other wlroots compositors:** install `grim`. Bind `snapline gui` in your compositor config, e.g. `bindsym Print exec snapline gui`.
- Install `wl-clipboard` so copied images stay in the clipboard reliably.
- The capture-cursor option needs X11 or the `grim` backend. The GNOME/KDE portal doesn't support it.

## Upload backends

- **Imgur:** create a Client ID at <https://api.imgur.com/oauth2/addclient> (anonymous usage) and paste it into *Settings → Upload*.
- **Custom HTTP:** sends a multipart `POST` to your URL. You can add headers and form fields. The link is read from the response with a JSON path (e.g. `data.link`), or from the whole body if the path is empty. This works with ShareX-style servers, Chevereto, Zipline, XBackBone and similar.

## Build from source

```
sudo apt install cmake ninja-build pkgconf qt6-base-dev qt6-svg-dev libgl-dev libx11-dev libxcb1-dev libxcb-xfixes0-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build
sudo cmake --install build
```

Requires Qt ≥ 6.4 and CMake ≥ 3.21. To build without X11 support, pass `-DSNAPLINE_X11=OFF`.

Packaging recipes are in `debian/` and `packaging/` (rpm, arch, flatpak, appimage, snap). CI in `.github/workflows/build.yml` builds all of them.

## License

GPL-3.0-or-later. See `LICENSE`.
