Region screenshots for Linux: press a hotkey, drag over what you want, mark it up, then copy, save, print or upload it. Works on X11 and Wayland (GNOME, KDE Plasma, Sway, Hyprland), multi-monitor and HiDPI.

## Download

| You use | Get | Install |
|---|---|---|
| Ubuntu 24.04 / Mint 22 / Pop!_OS 24.04 | `snapline_*_ubuntu24.04.deb` | `sudo apt install ./snapline_*_ubuntu24.04.deb` |
| Ubuntu 26.04 | `snapline_*_ubuntu26.04.deb` | `sudo apt install ./snapline_*_ubuntu26.04.deb` |
| Debian 13 (trixie) | `snapline_*_debian13.deb` | `sudo apt install ./snapline_*_debian13.deb` |
| Fedora | `snapline-*.fc*.x86_64.rpm` | `sudo dnf install ./snapline-*.fc*.x86_64.rpm` |
| openSUSE Tumbleweed | `snapline-*.opensuse.x86_64.rpm` | `sudo zypper install ./snapline-*.opensuse.x86_64.rpm` |
| Arch / Manjaro / EndeavourOS | `snapline-*.pkg.tar.zst` | `sudo pacman -U snapline-*.pkg.tar.zst` |
| Any distro (Flatpak) | `snapline.flatpak` | `flatpak install --user snapline.flatpak` |
| Any distro (portable) | `Snapline-*-x86_64.AppImage` | `chmod +x Snapline-*.AppImage && ./Snapline-*.AppImage` |

`SHA256SUMS` lists the checksum of every file: `sha256sum -c SHA256SUMS --ignore-missing`.

## Getting started

Run `snapline` (or open it from the app menu); it sits in the tray. `Print` selects an area, `Shift+Print` saves the full screen.

- **Wayland:** install `wl-clipboard`. On Sway/Hyprland also install `grim` and bind `snapline gui` to a key.
- **GNOME:** set your hotkeys in Snapline's *Settings → Hotkeys* (click the field, press the keys, OK). To use `Print`, confirm when Snapline asks to take it from GNOME's screenshot tool.
