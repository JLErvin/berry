<div align='center'>
    <h1>berry</h1><br>
</div>

A healthy, bite-sized window manager written in C over the XLib library.

![Screenshot](https://external-preview.redd.it/A8DWRA2txIQM8g_CpXPXAoC-wU7CSrjJO2UdCW8Nv7Y.png?auto=webp&s=3f65c783c54fd2df1ffe0be7a9f3dfa9ae54a22c)

* [Description](#Description)
* [Installation](#Installation)
* [Usage](#Usage)
    * [Basic](#Basic)
    * [Commands](#Commands)
    * [Monitors](#Monitors)
    * [Themes](#Themes)
    * [Fonts](#Fonts)
* [Credits](*Credits)
* [Screenshots](*Screenshots)

# Description
`berry` is a floating window manager that responds to X events and manages window decorations.

`berryc` is a small client that sends X events to manage windows and configuration settings.

Much like `bspwm` and `windowchef`, `berry` does not handle keyboard inputs.
Instead, a program like `sxhkd` is needed to translate input events to `berryc` commands.

# Installation

First, install the XLib and Xft C headers on your distribution. On Ubuntu/Debian, install the following packages:
```
libx11-dev
libxft-dev
```

Clone the repository and make it

``` bash
git clone https://github.com/JLErvin/berry
cd berry
make
sudo make install
```

Copy the sample configuration files (assuming your are in the berry directory)

``` bash
mkdir $HOME/.config/berry
cp example/sxhkdrc $HOME/.config/berry/sxhkdrc
cp example/autostart $HOME/.config/berry/autostart
```

If you are not using a display manager add the following to your `.xinitrc`

```bash
sxhkd -c $HOME/.config/berry/sxhkdrc &
exec berry
```

If you are using a display manager (`lightdm`, etc), create a file `berry.desktop` in `/usr/share/xsessions/`
``` ini
[Desktop Entry]
Encoding=UTF-8
Name=berry
Comment=berry - a small window manager
Exec=berry
Type=XSession
```

Arch Linux users can download via the AUR: [berry](https://aur.archlinux.org/packages/berry-git/)

```bash
yay -S berry-git
```

# Usage

## Basic

`berry` is controlled by its client program `berryc`.
Upon startup, `berry` will run the file `autostart`, located by default
in `XDG_CONFIG_HOME/berry/autostart`.
A sample autostart has been included to help you, please see the installation section.

## Commands

`berry` does not handle keyboard input on its own.
Instead, a program like `sxhkd` is needed to translate keypress events into commands.
`berry` is controlled through its client, `berryc`.
The window manager can be controlled using the following commands:

* `window_move` **`x y`**
    * move the current window by x and y, relatively
* `window_move_absolute` **`x y`**
    * move the current window to x and y
* `window_resize` **`w h`**
    * resize the current window by w and h, relatively
* `window_resize_absolute` **`w h`**
    * resize the current window to w and h
* `window_raise`
    * raise the given window 
* `window_monocle`
    * set the current window to fill the screen, maintains decorations
* `window_close`
    * close the current window
* `window_center`
    * center the current window, maintains current size
* `switch_workspace` **`i`**
    * switch view to the given workspace
* `send_to_workspace` **`i`**
    * send the current window the given workspace
* `fullscreen`
    * set the current window to fill the screen, removes decorations
* `snap_left`
    * snap the given window to the left side of the screen and fill vertically
* `snap_right`
    * snap the given window to the left side of the screen and fill vertically
* `cardinal_focus` **`1/2/3/4`**
    * change focus to the client in the specified direction
* `toggle_decorations`
    * toggle decorations for the current client
* `cycle_focus`
    * change focus to the next client in the stack
* `pointer_focus`
    * focus the window under the current pointer (used by `sxhkd`)

## Monitors

`berry` supports the use of multiple monitors.
Monitors are associated with workspaces.
To associate a monitor with a specific workspace, using the following `berryc` command:

* `save_monitor` **`i j`**
    * Associate the ith workspace to the jth monitor

The number associated with each monitor can be determined through an application like `xrandr`.
The following command will list all monitors and their associated numbers:
```bash
xrandr --listmonitors
```

## Theme

`berryc` also controls the appearance of `berry`.
The following commands are available:

* `focus_color` **`XXXXXX`**
    * Set the color of the outer border for the focused window
* `unfocus_color` **`XXXXXX`**
    * Set the color of the outer border for all unfocused windows
* `inner_focus_color` **`XXXXXX`**
    * Set the color of the inner border and title bar for the focused window
* `inner_unfocus_color` **`XXXXXX`**
    * Set the color of the inner border and title bar for all unfocused windows
* `text_focus_color` **`XXXXXX`**
    * Set the color of the title bar text for the focused window
* `text_unfocus_color` **`XXXXXX`**
    * Set the color of the title bar text for the unfocused window
* `border_width` **`XXXXXX`**
    * Set the border width, in pixels, of the outer border
* `inner_border_width` **`XXXXXX`**
    * Set the border width, in pixels, of the inner border
* `title_height` **`XXXXXX`**
    * Set the height of the title bar, does not include border widths
* `top_gap` **`XXXXXX`**
    * Set the offset at the top of the monitor (usually for system bars)

## Fonts

`berry` now supports title bar text that describes the contents of its respective window.
By default, this font is defined by `font-config`'s default `monospace` font.
To check what font this is, you can simply run:

```bash
fc-match monospace
```

Due to limitations on the X server, the font cannot be changed from `berryc`.
If you wish to use a font other than the default monospace one, simply change
the `DEFAULT_FONT` value located inside of `src/config.h` and recompile `berry`.
Upon logging out/back into an X session, you should see the changes. 

# Features

* Multiple desktops
* Keyboard-drive resizing and movement
* Command line client to control windows and decorations
* Double borders
* Title bars
* Left/right snapping
* Fullscreen/monocle mode

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
