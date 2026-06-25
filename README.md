# Status Check – TeamSpeak 3 Plugin

Ein TeamSpeak 3 Plugin für Gruppen-Ready-Checks mit Echtzeit-HUD-Overlay.  
Mitglieder können ihren Status (Not Ready / Standby / Ready) per Tastenkürzel setzen – alle sehen es sofort im HUD.

---

## Installation

1. Neueste `.ts3_plugin`-Datei aus den [Releases](../../releases/latest) herunterladen
2. Doppelklick auf die Datei → TeamSpeak 3 öffnet den Plugin-Installer automatisch
3. TeamSpeak 3 neu starten
4. Das Plugin erscheint unter **Plugins → Status Check**

> **Voraussetzungen:** TeamSpeak 3 Client (Windows 64-bit)

---

## Bedienung

### Status setzen
| Aktion | Standard-Tastenkürzel |
|---|---|
| Status cyclen (Not Ready → Standby → Ready) | `Ctrl+Shift+R` |

Eigene Tastenkürzel (inkl. Joystick, Stream Deck) lassen sich über  
**Plugins → Status Check → Einstellungen (Tastenzuweisung)** konfigurieren.

### HUD
Das Overlay erscheint automatisch beim ersten Statuswechsel.  
Manuell steuerbar über **Plugins → Status Check → HUD an / HUD aus**.

### Status-Bedeutung
| Symbol | Status |
|---|---|
| 🔴 Not Ready | Noch nicht bereit |
| 🟡 Standby | Bereit auf Abruf |
| 🟢 Ready | Einsatzbereit |

---

## Für Entwickler – Build aus dem Quellcode

### Voraussetzungen
- Windows 10/11 64-bit
- [Visual Studio 2022 Build Tools](https://aka.ms/vs/17/release/vs_BuildTools.exe) (Workload: „C++ Build Tools")
- [Qt 5.15.2 MSVC 2019 64-bit](https://www.qt.io/download-qt-installer)
- CMake 3.16+

### Build

```bat
git clone https://github.com/DEIN_USERNAME/status-check-ts3.git
cd status-check-ts3
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target package_ts3plugin
```

Die fertige Datei liegt danach unter `build/staffel_viper_readycheck.ts3_plugin`.

---

## Lizenz

Privates Projekt – Staffel Viper
