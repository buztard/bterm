# Bare Terminal

A really bare terminal emulator based on [Vte](https://git.gnome.org/browse/vte)
without any special features, keybindings or custom configuration options.


## Why?

Because [Neovim](https://github.com/neovim/neovim) just added support for
[true color terminals](https://github.com/neovim/neovim/pull/2198) and I was
looking for a lightweight terminal that supports 24bit colors. (Well, I know, a
terminal with a Gtk3 dependency shouldn't be called *lightweight*)


## TODO

- Minimal configuration system
- Keyboard driven yank/put (or copy/paste if you prefer this term)
- Probably native Neovim integration through sockets.
