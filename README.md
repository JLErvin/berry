<div align='center'>
    <h1>berry</h1><br>
</div>

A healthy, bite-sized window manager written in C over the XLib library.

![Screenshot](https://i.imgur.com/J7nwibt.png)

# Description
`berry` is a floating window manager that responds to X events and manages window decorations.

`berryc` is a small client that sends X events to manage windows and configuration settings.

Much like `bspwm` and `windowchef`, `berry` does not handle keyboard inputs.
Instead, a program like `sxhkd` is needed to translate input events to `berryc` commands.

# Installation

First, install the XLib C headers on your distribution. On Ubuntu/Debian, for example, they are available as `libx11-dev`

Clone the repository and make it

``` bash
git clone https://github.com/JLErvin/berry
cd berry
make
sudo make install
```

Copy the sample configuration files

``` bash
mkdir $HOME/.config/berry
cp berry/examples/sxhkdrc $HOME/.config/berry/sxhkdrc
cp berry examples/autostart $HOME/.config/berry/autostart
```

Add the following to your `.xinitrc`

```bash
sxhkd -c $HOME/.config/berry/sxhkdrc &
exec berry
```

Arch Linux users can download via the AUR: [berry](https://aur.archlinux.org/packages/berry-git/)

```bash
yay -S berry
```

# Usage

`berry` relies on a program like `sxhkd` to handle keypress events. 
You can use `man berryc` to view available commands and bind them to 
your favorite keystrokes in `sxhkdrc`. 

`berry` now supports moving windows using the mouse.
To use this feature, you must be using `sxhkd` version `0.5.5` or **earlier**.
Use the following bindings in your `sxhkdrc` to allow moving windows using the pointer:

```bash
# Focus a window by clicking, no modifier needed
~button1
    berryc pointer_move 0

# Move windows by holding down alt
alt + ~button1
    berryc pointer_move 1

# Needed to update berry's internal state
    berryc pointer_move 2
```

# Features

* Multiple desktops
* Keyboard-drive resizing and movement
* Command line client to control windows and decorations
* Double borders
* Title bars
* Left/right snapping
* Fullscreen/monocle mode

# TODO

* Titlebar window text
* Add more EWMH/ICCCM protocols

# Credits

Although I wrote `berry` on my own, it was inspired by people much smarter than I. 
A very special thanks to the following people for writing great code and open-sourcing it!

* Tudurom's [windowchef](https://github.com/tudurom/windowchef)
* Vain's [katriawm](https://github.com/vain/katriawm)

# Contributors

Special thanks to the following people for contributing:

* [Kyle G](https://www.reddit.com/user/FlashDaggerX): AUR Package Maintainer

# Additional Screenshots

Double borders with titlebars:

![Imgur](https://i.imgur.com/oRk9xzq.png)

Monocle Mode

![Imgur](https://i.imgur.com/DTYhOLR.png)
