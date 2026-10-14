<div align="center">

# 🛰️ IP Inspector Pro

**Know your network. At a glance.**

A fast, modern **Qt6 desktop application** that reveals everything about your
network identity — your local and public IP, geolocation, MAC addresses,
hostnames, and every network interface — wrapped in a clean, cross-platform UI.

![Qt6](https://img.shields.io/badge/Qt-6.x-41CD52?style=flat-square&logo=qt)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-9b59b6?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-27ae60?style=flat-square)

**IP Inspector Pro** — because `ip a` is fine, but this looks better. ☕

</div>

---

## ✨ Features

- 📍 **Local IP + Public IP** — instantly detect your internal address and your
  external-facing public IP via a background async request (UI never freezes).
- 🗺️ **Geolocation** — city, region, country, and organization for your public IP.
- 🖧 **Network Interfaces table** — every interface with IP, MAC, status, and type,
  with a live **filter/search box**.
- 💻 **System info** — hostname and MAC address in one glance.
- 🌙 **Dark / Light themes** — easy on the eyes, whichever way you roll.
- 🔄 **Auto-refresh** — keep everything up to date on an interval (togglable).
- 📤 **Export** — dump your full network report to a file.
- 🔖 **System tray** — minimize to tray, quick access from anywhere.
- 📋 **Copy-to-clipboard** — one click to copy any IP.

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
- **Windows:** Qt 6 (MinGW 64-bit) toolchain, e.g. `6.7.0`.

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

Using the provided script from a Qt command prompt:

```bat
build.bat
```

Or manually:

```bat
qmake6 ipchecker.pro -spec win32-g++ CONFIG+=release
mingw32-make -j4
windeployqt6 --release --no-translations --no-opengl-sw release\ipchecker.exe
```

---

## 📦 Installation (prebuilt binaries)

> Prebuilt binaries are produced automatically by the GitHub Actions pipeline
> and attached to each **release**.

### Linux — `.deb`

```bash
sudo apt install ./ipchecker_1.0.0_amd64.deb
```

### Windows — portable zip

1. Download `ipchecker_1.0.0_windows_portable.zip` from the release.
2. Extract anywhere.
3. Run `ipchecker.exe` (no installer required).

---

## 🚀 CI / CD

`.github/workflows/build.yml` automatically builds on every `v*` tag and on
manual dispatch:

| Job            | Output                              |
|----------------|-------------------------------------|
| `build-linux-deb` | `ipchecker_1.0.0_amd64.deb`      |
| `build-windows-exe` | `ipchecker_1.0.0_windows_portable.zip` |
| `create-release`   | Auto-generates a GitHub Release      |

Releases are created when you push a version tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

---

## 🗺️ Roadmap

- [x] Core IP / network info dashboard
- [x] Dark & light themes
- [x] System tray + auto-refresh
- [x] Export + interface filtering
- [x] IPv6 support (IPv4 & IPv6 addresses and public IP)
- [ ] Persistent IP history viewer
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
This project is under construction.
---
<div align="center">
Made with ❤️ and **Qt** • [IP Inspector Pro](https://github.com/amg/ipchecker)
</div>