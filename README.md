# XWeather

A contemporary X11/Motif weather application, inspired by [Gnome Weather](https://apps.gnome.org/Weather/). Runs great on [SGI IRIX](https://en.wikipedia.org/wiki/IRIX), MacOS, and Linux.

![xweather screenshot](docs/screenshots/xweather_daily.png) 

![xweather screenshot](docs/screenshots/xweather_hourly.png)

## Features

- Live weather data from open-meteo.com
- Daily and hourly forecast views
- Easily manage and switch between multiple location
- Written in C and X11/Motif for maximum compatibility with vintage UNIXes.

## Installation

### MacOS

xweather runs on macOS using [XQuartz](https://www.xquartz.org/) as the X server and [Homebrew](https://brew.sh/) to supply the Motif/X11 development libraries (macOS ships neither).

**1. Install XQuartz** (the X server) if you don't already have it:

```sh
brew install --cask xquartz
```

Log out and back in (or reboot) after the first XQuartz install so the `DISPLAY` environment and X11 app registration take effect.

**2. Install the build dependencies** with Homebrew:

```sh
brew install openmotif json-c libxpm
```

This pulls in Motif (`Xm`) and its X11/Xt/Xp dependencies, `json-c`, and `libXpm`. `libcurl` and `pkg-config` ship with Xcode Command Line Tools / macOS and don't need a separate install, but make sure the Command Line Tools are installed:

```sh
xcode-select --install
```

**3. Configure and build:**

```sh
./configure
make
```

`configure` detects macOS automatically and wires up the Homebrew include/library paths for Motif, X11, and libXpm.

**4. Run it.** Make sure XQuartz is running (launch it once from Spotlight/Applications, or it will auto-start when an X11 app connects), then:

```sh
./xweather
```

## Acknowledgements 

- Thanks to the folks at [Gnome Weather](https://apps.gnome.org/Weather/) for visual inspiration and a great set of [weather icons](./assets/icons).