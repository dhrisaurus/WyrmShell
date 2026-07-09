# wyrmshell

Custom GTK3 + VTE terminal emulator with an ember/obsidian dragon theme.

## Layout

- **Left**: your interactive main shell. Its rc-file hook prints just the
  dragon on startup (a plain `cat` of a pre-colored ANSI file) -- no
  sysinfo text here anymore.
- **Right column, top**: a small fixed-height panel that runs
  `fastfetch --logo none` once at launch -- kernel/uptime/cpu/gpu/disk/
  memory + color swatches, no logo (the logo lives in the left pane).
- **Right column, bottom**: a live `python3` REPL, starting right where
  the sysinfo panel ends -- real widget stacking, not a guessed offset,
  so it lines up correctly regardless of window size.

## 1. Build dependencies

wyrmshell itself is plain GTK3 + VTE + glibc -- nothing SUSE/distro-specific
in the code. The only distro-specific part is *installing* the dev
packages, since names differ. Easiest way:

```bash
bash scripts/install-deps.sh
```

This detects zypper / dnf / apt / pacman / apk and installs the right
package names automatically. If your distro/package manager isn't one of
those, or you'd rather do it by hand:

| Distro family          | Command |
|---|---|
| openSUSE                | `sudo zypper install gcc make gtk3-devel vte-devel fastfetch python3` |
| Fedora / RHEL / CentOS  | `sudo dnf install gcc make gtk3-devel vte291-devel fastfetch python3` |
| Debian / Ubuntu         | `sudo apt install gcc make libgtk-3-dev libvte-2.91-dev fastfetch python3` |
| Arch / Manjaro          | `sudo pacman -S gcc make gtk3 vte3 fastfetch python` |
| Alpine                  | `sudo apk add gcc make gtk+3.0-dev vte3-dev fastfetch python3` |

**Note on fastfetch**: it's in most distros' repos now, but if yours has an
older version without it, grab a release directly from
https://github.com/fastfetch-cli/fastfetch/releases

Verify GTK/VTE headers are visible to the build regardless of how you
installed them:
```bash
pkg-config --exists gtk+-3.0 vte-2.91 && echo OK || echo MISSING
```

## 2. Build wyrmshell

```bash
make
./wyrmshell        # test before installing
```

## 3. Install wyrmshell

```bash
make install
```

Installs the binary to `~/.local/bin/wyrmshell`, registers a `.desktop`
launcher, and copies (only if not already present):
- `~/.config/fastfetch/config.jsonc` + `dragon-logo.txt` (used by the
  sysinfo panel)
- `~/.config/wyrmshell/dragon-ansi.txt` (the plain-ANSI dragon, `cat`'d
  directly in the main shell -- decoupled from fastfetch's templating)

## 4. fastfetch

Already covered by `scripts/install-deps.sh` / the table above if you
haven't installed it yet.

## 5. Wire the dragon into your shell startup

wyrmshell sets `WYRMSHELL=1` in the environment of every shell it spawns,
so this only fires inside wyrmshell -- not in other terminals.

Check which shell you're on:
```bash
echo $SHELL
```

If bash, append to `~/.bashrc`:
```bash
echo -e '\nif [ -n "$WYRMSHELL" ]; then\n    cat ~/.config/wyrmshell/dragon-ansi.txt\nfi' >> ~/.bashrc
```
(swap `.bashrc` for `.zshrc` if you're on zsh)

**Note:** if you followed the earlier version of this README, you may
already have a line that runs plain `fastfetch` in `.bashrc` -- remove or
comment that out, since sysinfo now lives in its own panel instead.

## Customizing

- **Dragon (main pane)**: edit `~/.config/wyrmshell/dragon-ansi.txt`
  directly -- it's your ASCII art with a raw ANSI color code at the very
  start and a reset at the end. Edit the art freely; leave those two
  escape bits alone (or change the color code, `\x1b[38;5;208m` is 256-color
  amber -- swap `208` for any 0-255 value).
- **Sysinfo panel fields/colors**: edit `~/.config/fastfetch/config.jsonc`.
  Run `fastfetch --list-modules` to see everything available.
- **Sysinfo panel height**: `SYSINFO_HEIGHT_PX` in `main.c` (currently 300).
  If the fastfetch output is taller or shorter than that on your system,
  bump this number up/down and rebuild.
- **Terminal theme (background/text colors)**: `set_colors()` in `main.c`.

## Keybinds

- `Ctrl+Shift+C` / `Ctrl+Shift+V` -- copy / paste (main pane)
- `Ctrl+Shift+Q` -- quit
- `Ctrl +` / `Ctrl -` / `Ctrl 0` -- font zoom (main pane)
- `Ctrl+Shift+P` -- toggle the whole right column (sysinfo + python panel)

## Python panel

- It's a real live `python3` process -- type code, get `>>>` results
  immediately, exactly like running `python3` in any terminal.
- Nothing touches disk from normal use.
- **Save as script** button: asks for confirmation first. On yes, it
  extracts the code you actually typed (drops prompts and printed
  output), writes it to `wyrm_scratch.py` in your main shell's *live*
  current directory (tracked via `/proc/<pid>/cwd`, so it follows `cd`),
  and runs it inside the same REPL session via `exec()` -- output appears
  right there, and `if __name__ == "__main__":` blocks still work.

## What's next (not built yet)

- Tabs / multiple split panes
- Claude Code / Codex quick-launch pane
- Quake-style drop-down, session save/restore, fuzzy history search
