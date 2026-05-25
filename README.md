# ClickBait

A small Windows tray app that highlights your mouse clicks during live demos, reviews, bug reports, and recordings.

Screen recorders can add click effects after the fact. ClickBait is for the moments happening live, when you want people to clearly see what you are clicking on screen.

## Demo

Icon preview:

![ClickBait icon](windows/assets/ClickBait.png)

## Use Cases

- Live product demos where viewers need to follow exactly what you clicked
- UX reviews where the delay between click and response matters
- Bug reports where a recording should show both the action and the app behavior
- Tutorials, workshops, and conference talks where pointer movement alone is easy to miss
- Screen recordings where you want click feedback visible in real time

## Features

- Global click highlights across Windows apps
- Separate visuals for press, release, right-click, and drag
- Tray-menu controls for size, duration, intensity, and colors
- Small tray icon that opens the settings menu
- Compact tray icon option
- Visible on/off tray icon states
- Test pulse at the current pointer location
- Saved settings under `%APPDATA%\ClickBait`
- Native Win32 app with a very small binary footprint

## Build On Windows

Build the app with:

```bat
windows\build-clickbait.bat
```

The output binary is:

```text
windows\dist\ClickBait.exe
```

The current build is produced from [windows/ClickBait.cpp](/C:/Users/User/Documents/ClickBait/windows/ClickBait.cpp), [windows/ClickBait.rc](/C:/Users/User/Documents/ClickBait/windows/ClickBait.rc), and [windows/build-clickbait.bat](/C:/Users/User/Documents/ClickBait/windows/build-clickbait.bat).

## How It Works

ClickBait runs as a tray app. Left-click the tray icon to open the settings window, or right-click it for the quick menu.

From there you can:

- Enable or disable click highlighting
- Choose which events are shown
- Change size, intensity, duration, and color
- Trigger a test pulse
- Quit the app

The tray icon also reflects state:

- Bright icon when ClickBait is enabled
- Muted slashed icon when ClickBait is disabled

## Permissions

ClickBait uses a low-level mouse hook on Windows to observe global mouse activity.

## Project Notes

This repository contains the Windows ClickBait app only.
