<div align="center">

# 🛰️ IP Inspector Pro

**Know your network. At a glance.**

A fast, modern **Qt6 desktop application** that reveals everything about your
network identity — your local and public IP, geolocation, MAC addresses,
hostnames, and every network interface — wrapped in a clean, cross-platform UI.

![Qt6](https://img.shields.io/badge/Qt-6.x-41CD52?style=flat-square&logo=qt)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20macOS-9b59b6?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-27ae60?style=flat-square)
[![Build & Release](https://github.com/amgpulse/ipchecker/actions/workflows/build.yml/badge.svg)](https://github.com/amgpulse/ipchecker/actions/workflows/build.yml)

**IP Inspector Pro** — because `ip a` is fine, but this looks better. ☕

</div>

---

## ✨ Features

- 📍 **Local IP + Public IP** — instantly detect your internal address (IPv4 &
  IPv6, picked via the OS routing table) and your external-facing public IP via
  background async requests with timeouts (the UI never freezes).
- 🗺️ **Geolocation** — city, region, country, and organization for your public
  IP, cached so it is only re-fetched when your IP actually changes.
- 🖧 **Network Interfaces table** — every interface with IP, MAC, status, and
  type, sortable, with a live **filter/search box** across all columns.
- 💻 **System info** — hostname and MAC address in one glance.
- 🌙 **Dark / Light themes** — easy on the eyes, whichever way you roll.
- 🔄 **Auto-refresh** — configurable interval (10s / 30s / 1m / 5m / 15m),
  toggglable from the toolbar.
- 🕘 **IP history** — every public IP change is recorded with a timestamp and
  shown in a history dialog (stored locally in your settings).
- 📤 **Export** — save a full network report as **plain text or real JSON**.
- 🔖 **System tray** — closing the window keeps the app running in the tray;
  quit from the tray menu.
- 📋 **Copy-to-clipboard** — one click (or double-click a table row) to copy.
- 🧩 **Single instance** — launching the app again focuses the running window.

---

## 🛠️ Tech Stack

| Layer    | Technology                        |
|----------|-----------------------------------|
| GUI      | Qt Widgets (Qt 6.x)               |
| Language | C++17                             |
| Network  | Qt Network (`QNetworkAccessManager`) |
| Build    | qmake (`.pro`) / CI via GitHub Actions |

---

## ✅ Requirements

- **Linux (Debian/Ubuntu based):** Qt 6 base dev packages + a C++17 compiler.
- **Windows:** Qt 6 (MinGW 64-bit or MSVC) toolchain, e.g. `6.7.0`.
- **macOS:** Qt 6 + Xcode command line tools.

---

## 🔨 Build from source

### Linux (Debian/Ubuntu)

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y qt6-base-dev build-essential

# 2. Build
qmake6 ipchecker.pro
make -j$(nproc)

# 3. Run
./ipchecker
```

### Windows

Using the provided script from a Qt command prompt (run it from anywhere — it
switches to the repository root itself):

```bat
packaging\windows\build.bat
```

Or manually:

```bat
qmake ipchecker.pro -spec win32-g++ CONFIG+=release
mingw32-make -j4
windeployqt --release --no-translations release\ipchecker.exe
```

### macOS

```bash
qmake ipchecker.pro
make -j"$(sysctl -n hw.ncpu)"
macdeployqt ipchecker.app
open ipchecker.app
```

---

## 📦 Installation (prebuilt binaries)

> Prebuilt binaries are produced automatically by the GitHub Actions pipeline
> and attached to each **release** — see the
> [releases page](https://github.com/amgpulse/ipchecker/releases).

### Linux — `.deb` (Ubuntu 24.04+)

```bash
sudo apt install ./ipchecker_<version>_amd64.deb
```

### Linux — `.rpm` (Fedora)

```bash
sudo dnf install ./ipchecker-<version>-1.x86_64.rpm
```

### Windows — portable zip

1. Download `ipchecker_<version>_windows_x64.zip` from the release.
2. Extract anywhere.
3. Run `ipchecker.exe` (no installer required).

### Windows — installer

Download and run `ipchecker_<version>_setup.exe` from the release.

### macOS

Download `ipchecker_<version>_macos.dmg` (or the `.zip`) from the release,
open the app, and move it to `/Applications`.

---

## 🚀 CI / CD — automatic releases

`.github/workflows/build.yml` runs on **every push to `main`**, on every `v*`
tag, and on pull requests (build-only):

| Job            | Output                                              |
|----------------|-----------------------------------------------------|
| `build-linux-deb` | `ipchecker_<version>_amd64.deb` (+ headless smoke test) |
| `build-linux-rpm` | `ipchecker-<version>-1.x86_64.rpm`                |
| `build-windows`   | `ipchecker_<version>_windows_x64.zip` + `ipchecker_<version>_setup.exe` |
| `build-macos`     | `ipchecker_<version>_macos.zip` + `ipchecker_<version>_macos.dmg` |
| `release`         | Creates / updates the GitHub Release automatically  |

Release behaviour:

- **Push to `main`** → the rolling **`continuous`** prerelease is updated:
  its tag is moved to your new commit and all assets are replaced.
- **Push a version tag** (`v*`) → a full, non-prerelease release is created
  with generated release notes.

```bash
# Cut a release
git tag v1.2.0
git push origin v1.2.0
```

The app version lives in **one place** — `VERSION` in `ipchecker.pro` — and CI
reads it from there for every package and release.

---

## 🔒 Privacy

IP Inspector Pro talks to exactly two third-party services:

| Data           | Service           | Why                                  |
|----------------|-------------------|--------------------------------------|
| Public IP      | `api.ipify.org`   | To discover your public IPv4/IPv6    |
| Geolocation    | `ipapi.co`        | City/region/country/org for that IP  |

Nothing else leaves your machine, geolocation lookups are cached, and the
information is also shown in the app itself.

---

## 🗺️ Roadmap

- [x] Core IP / network info dashboard
- [x] Dark & light themes
- [x] System tray + configurable auto-refresh
- [x] Export (text/JSON) + interface filtering
- [x] IPv6 support (IPv4 & IPv6 addresses and public IP)
- [x] Persistent IP history viewer
- [x] Single-instance support + close-to-tray
- [x] Automatic multi-platform releases (Linux, Windows, macOS)
- [ ] Who-is & DNS lookup
- [ ] Network speed / latency test

---

## 🤝 Contributing

Pull requests are welcome! For major changes, please open an issue first to
discuss what you'd like to change.

1. Fork the repo
2. Create your feature branch (`git checkout -b feature/awesome`)
3. Commit your changes
4. Push to the branch (`git push origin feature/awesome`)
5. Open a pull request

---

## 📄 License

Distributed under the **MIT License**. See `LICENSE` for more information.

---

<div align="center">
Made with ❤️ and **Qt** • [IP Inspector Pro](https://github.com/amgpulse/ipchecker)
</div>
