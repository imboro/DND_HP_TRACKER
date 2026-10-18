# HP Tracker for Mac

**[⬇ Download HP-Tracker-Mac.zip](https://github.com/imboro/DND_HP_TRACKER/raw/main/Downloads/Mac/HP-Tracker-Mac.zip)**

For Macs with Apple Silicon (M1, M2, M3, M4...). Nothing to install.
Check yours in the Apple menu > **About This Mac**: "Chip: Apple M…" means it works; an "Intel"
processor means this version won't run.

## Open it

1. Download the zip (link above) and double-click it to unzip it.
2. Drag **HP Tracker** into your **Applications** folder (optional, but tidy).
3. Open it. The first time, macOS says it can't check the app, because it isn't
   signed by a paid Apple developer account:
   - **Right-click** the app > **Open** > **Open**.
   - On newer macOS (15 Sequoia and later) that may not show an Open button. Then go
     to the Apple menu > **System Settings > Privacy & Security**, scroll down, and click
     **Open Anyway** next to "HP Tracker".
   - Still refused? Run this in Terminal, then open it again:
     `xattr -cr "/Applications/HP Tracker.app"`
4. If macOS asks whether to accept incoming network connections, click **Allow**.
   That's only needed for the player screens (phones).

## Good to know

- Your encounter saves by itself, in
  `~/Library/Application Support/HotelWorkshop/HPTracker/`.
- Shortcuts use **Cmd**: Cmd+Z undo, Cmd+Shift+Z redo, Cmd+A select all,
  Cmd+D duplicate, Delete removes the selected cards, Space passes the turn.
- To update, download the new zip and replace the old app. Your saved encounter,
  party and library are kept.

[← Back to the downloads](../)
