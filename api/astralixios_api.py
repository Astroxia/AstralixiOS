"""
astralixios_api.py  -  AstralixiOS TUI Application API
=======================================================
The official API for building TUI apps on AstralixiOS.

TUI stands for Terminal User Interface. It is basically an advanced CLI
with very basic graphics - coloured text, borders, boxes, and widgets,
all drawn inside the terminal using characters. No windows, no GUI.

RULES
-----
- Python Standard Library ONLY. No pip, no third-party packages.
- TUI ONLY. No tkinter windows, no turtle, no pygame, nothing graphical.
- Your entire app must be a single .py file.
- Import this module and use its functions. Do not write raw curses code.
- Your app MUST provide a way for the user to exit (e.g. Q or Escape).
  The API will exit on Q or Escape by default, but you should make this
  obvious to the user - put it in the status bar or somewhere visible.

DISPLAY MODEL
-------------
AstralixiOS runs on a fixed 720p display. There is no window resizing at
runtime. The TUI grid maps to approximately 213 columns x 45 rows
(at 6x16 px per character cell).

app.cols and app.rows reflect the actual terminal size measured at startup.
Use them (not hardcoded numbers) for all layout calculations so your app
looks correct regardless of the exact terminal dimensions.

QUICK START
-----------
    from astralixios_api import App, draw_box, draw_text, draw_status_bar, run

    def draw(app):
        draw_box(app, 0, 0, app.cols, app.rows, title="My App", double_border=True)
        draw_text(app, 4, 2, "Hello, AstralixiOS!")
        draw_status_bar(app, " Q to quit")

    app = App(title="My App")
    run(app, draw)
"""

import curses
import os
import shutil
import time


# ---------------------------------------------------------------------------
# CONSTANTS - colour palette indices (standard ANSI, always available)
# ---------------------------------------------------------------------------

COLOR_BLACK   = 0   # curses.COLOR_BLACK
COLOR_RED     = 1   # curses.COLOR_RED
COLOR_GREEN   = 2   # curses.COLOR_GREEN
COLOR_YELLOW  = 3   # curses.COLOR_YELLOW
COLOR_BLUE    = 4   # curses.COLOR_BLUE
COLOR_MAGENTA = 5   # curses.COLOR_MAGENTA
COLOR_CYAN    = 6   # curses.COLOR_CYAN
COLOR_WHITE   = 7   # curses.COLOR_WHITE

# Text attribute flags. Combine them with the | operator.
# Example: ATTR_BOLD | ATTR_UNDERLINE
ATTR_NORMAL    = 0          # plain text, no effects
ATTR_BOLD      = 1 << 0     # bold / bright
ATTR_DIM       = 1 << 1     # dim / faint
ATTR_UNDERLINE = 1 << 2     # underlined
ATTR_BLINK     = 1 << 3     # blinking (not all terminals support this)
ATTR_REVERSE   = 1 << 4     # swap foreground and background colours
ATTR_ITALIC    = 1 << 5     # italic (not all terminals support this)

# 720p reference grid dimensions (6px wide, 16px tall per character cell)
REF_COLS = 213   # 1280 / 6  = ~213 columns
REF_ROWS = 45    #  720 / 16 =  ~45 rows

# Spinner frame sequences used by draw_spinner()
_SPINNERS = {
    "braille": list("⣾⣽⣻⢿⡿⣟⣯⣷"),
    "dots":    list("⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"),
    "line":    list("|/-\\"),
    "box":     list("◰◳◲◱"),
}

# Box-drawing character sets used by draw_box()
_BOX_SINGLE = {"tl": "┌", "tr": "┐", "bl": "└", "br": "┘", "h": "─", "v": "│"}
_BOX_DOUBLE = {"tl": "╔", "tr": "╗", "bl": "╚", "br": "╝", "h": "═", "v": "║"}

# Key name lookup table used by key_name()
_KEY_NAMES = {
    curses.KEY_UP:        "UP",
    curses.KEY_DOWN:      "DOWN",
    curses.KEY_LEFT:      "LEFT",
    curses.KEY_RIGHT:     "RIGHT",
    curses.KEY_ENTER:     "ENTER",
    curses.KEY_BACKSPACE: "BACKSPACE",
    curses.KEY_DC:        "DELETE",
    curses.KEY_HOME:      "HOME",
    curses.KEY_END:       "END",
    curses.KEY_PPAGE:     "PAGE_UP",
    curses.KEY_NPAGE:     "PAGE_DOWN",
    curses.KEY_F1:        "F1",
    curses.KEY_F2:        "F2",
    curses.KEY_F3:        "F3",
    curses.KEY_F4:        "F4",
    curses.KEY_F5:        "F5",
    curses.KEY_F6:        "F6",
    curses.KEY_F7:        "F7",
    curses.KEY_F8:        "F8",
    curses.KEY_F9:        "F9",
    curses.KEY_F10:       "F10",
    curses.KEY_F11:       "F11",
    curses.KEY_F12:       "F12",
    10:                   "ENTER",    # LF  (Linux Enter)
    13:                   "ENTER",    # CR  (Windows Enter)
    9:                    "TAB",
    27:                   "ESC",
    127:                  "BACKSPACE",
}


# ---------------------------------------------------------------------------
# INTERNAL HELPERS - not part of the public API, do not call these directly
# ---------------------------------------------------------------------------

def _curses_attr(app, fg, bg, attr):
    """
    Convert fg/bg colour constants and ATTR_* flags into a curses attribute int.
    Called internally by every draw_* function. Do not call this directly.
    """
    pair_id = fg * 8 + bg + 1          # encode fg/bg into a unique pair ID
    pair_id = max(1, min(pair_id, 63))  # clamp to valid range
    try:
        curses.init_pair(pair_id, fg, bg)
        result = curses.color_pair(pair_id)
    except Exception:
        result = 0

    if attr & ATTR_BOLD:      result |= curses.A_BOLD
    if attr & ATTR_DIM:       result |= curses.A_DIM
    if attr & ATTR_UNDERLINE: result |= curses.A_UNDERLINE
    if attr & ATTR_BLINK:     result |= curses.A_BLINK
    if attr & ATTR_REVERSE:   result |= curses.A_REVERSE
    if attr & ATTR_ITALIC:
        try:    result |= curses.A_ITALIC
        except: pass   # silently skip if the terminal doesn't support italic

    return result


def _safe_addstr(win, row, col, text, attr=0):
    """
    Write text to a curses window, ignoring out-of-bounds writes.
    curses raises an exception when writing to the bottom-right corner cell;
    this wrapper catches it so your app doesn't crash.
    """
    try:
        win.addstr(row, col, text, attr)
    except curses.error:
        pass


# ---------------------------------------------------------------------------
# App - the central application object
# ---------------------------------------------------------------------------

class App:
    """
    The root application object. Create one at the top of your app and pass
    it to every API function.

    Attributes
    ----------
    title   : str   - the app name, shown in the title bar if you draw one
    cols    : int   - terminal width in character columns (measured at startup)
    rows    : int   - terminal height in character rows   (measured at startup)
    scale_x : float - horizontal scale factor vs. the 720p reference grid
    scale_y : float - vertical   scale factor vs. the 720p reference grid
    running : bool  - True while the main loop is active

    Args
    ----
    title : str, optional
        The app name. Defaults to "AstralixiOS App".
    fps : int, optional
        Target frames per second for the draw loop. Default is 30.
        Lower = less CPU. Higher = snappier feel, though above 30 is rarely needed.
    """

    def __init__(self, title: str = "AstralixiOS App", fps: int = 30):
        self.title   = title
        self.fps     = max(1, fps)
        self.running = False
        self._win    = None          # curses stdscr, assigned inside run()
        self._notifications = []     # list of (message, expiry, fg, bg) tuples

        # Measure the terminal once at startup.
        # AstralixiOS does not resize at runtime, so this stays fixed.
        size = shutil.get_terminal_size(fallback=(REF_COLS, REF_ROWS))
        self.cols    = size.columns
        self.rows    = size.lines
        self.scale_x = self.cols / REF_COLS
        self.scale_y = self.rows / REF_ROWS


# ---------------------------------------------------------------------------
# LIFECYCLE
# ---------------------------------------------------------------------------

def run(app: App, draw_fn, input_fn=None) -> None:
    """
    Start the application main loop. This blocks until the app exits.

    Handles all curses setup and cleanup for you. You do not need to call
    curses.wrapper() or curses.endwin() yourself.

    Q and Escape always quit the app. Make sure your status bar or UI makes
    this clear to the user - every AstralixiOS app must have a visible exit
    option.

    Args
    ----
    app      : App
        The App instance you created with App().
    draw_fn  : callable(app) -> None
        Your draw function. Called once per frame. All draw_* calls go here.
    input_fn : callable(app, key: int) -> None, optional
        Your input handler. Called on every keypress. `key` is a raw curses
        key code - pass it through key_name() or is_printable() to use it.
        If you skip this, only Q/Escape will do anything (quitting the app).
    """
    def _main(stdscr):
        app._win = stdscr

        # Hide the text cursor - AstralixiOS TUI apps do not use a visible cursor.
        # Mouse support is not available yet; it will be added in a future version
        # and will be limited to scrolling only.
        curses.curs_set(0)

        # Non-blocking input - getch() returns -1 immediately if no key is pressed
        stdscr.nodelay(True)

        # Enable special keys like arrows and F-keys
        stdscr.keypad(True)

        curses.start_color()
        curses.use_default_colors()

        # Pre-initialise all 64 standard colour pairs up front so draw_* functions
        # do not have to worry about initialising them on demand
        for fg in range(8):
            for bg in range(8):
                pair_id = fg * 8 + bg + 1
                if pair_id <= 63:
                    try:
                        curses.init_pair(pair_id, fg, bg)
                    except Exception:
                        pass

        app.running = True
        frame_time  = 1.0 / app.fps

        while app.running:
            t0 = time.monotonic()

            # Drain all pending key events for this frame
            while True:
                key = stdscr.getch()
                if key == -1:
                    break
                # Q or Escape always quit - this is required behaviour
                if key in (ord('q'), ord('Q'), 27):
                    app.running = False
                    break
                if input_fn is not None:
                    input_fn(app, key)

            if not app.running:
                break

            # Erase the previous frame then draw the new one
            stdscr.erase()
            draw_fn(app)

            # Draw any active notification banners on top of everything else
            _draw_notifications(app)

            stdscr.refresh()

            # Sleep out the rest of the frame budget
            elapsed   = time.monotonic() - t0
            remaining = frame_time - elapsed
            if remaining > 0:
                curses.napms(int(remaining * 1000))

    curses.wrapper(_main)


def _draw_notifications(app: App) -> None:
    """
    Internal: renders active notification banners and clears expired ones.
    Called automatically at the end of each frame by run(). Do not call this.
    """
    now   = time.monotonic()
    alive = [(m, e, f, b) for (m, e, f, b) in app._notifications if now < e]
    app._notifications = alive

    if alive:
        msg, expiry, fg, bg = alive[-1]   # show the most recent one
        text = f"  {msg}"[:app.cols].ljust(app.cols)
        _safe_addstr(app._win, app.rows - 1, 0, text,
                     _curses_attr(app, fg, bg, ATTR_BOLD))


def quit(app: App) -> None:
    """
    Tell the main loop to stop after the current frame finishes.

    You can call this from your input_fn or draw_fn when you want to exit
    programmatically (e.g. a "Quit" menu item). Q and Escape do this
    automatically without you needing to call quit() yourself.

    Args
    ----
    app : App
    """
    app.running = False


def get_size(app: App) -> tuple:
    """
    Return the terminal dimensions as (cols, rows).

    Since AstralixiOS does not resize at runtime, this always returns the
    same values that were measured when App() was created. It is here mainly
    for clarity and convenience.

    Args
    ----
    app : App

    Returns
    -------
    (cols: int, rows: int)
    """
    return (app.cols, app.rows)


def scale(app: App, ref_col: int = 0, ref_row: int = 0) -> tuple:
    """
    Convert a position from the 720p reference grid (213x45) to actual
    terminal coordinates.

    Design your layout using reference grid positions, then call scale()
    to get the real coordinates. This means your app will look correct
    on any terminal that differs slightly from the 720p reference.

    Args
    ----
    app     : App
    ref_col : int - column in the 720p reference grid (0-based)
    ref_row : int - row    in the 720p reference grid (0-based)

    Returns
    -------
    (col: int, row: int) scaled to the actual terminal size

    Example
    -------
        col, row = scale(app, 106, 22)   # roughly the centre of the screen
    """
    return (int(ref_col * app.scale_x), int(ref_row * app.scale_y))


# ---------------------------------------------------------------------------
# DRAWING - TEXT
# ---------------------------------------------------------------------------

def draw_text(app: App, col: int, row: int, text: str,
              fg=COLOR_WHITE, bg=COLOR_BLACK, attr=ATTR_NORMAL,
              scaled: bool = False) -> None:
    """
    Draw a single line of text at a position on screen.

    Text is clipped automatically if it goes past the right edge.
    Newlines inside the string are NOT handled - use draw_multiline() for that.

    Args
    ----
    app    : App
    col    : int  - column to start at (0 = left edge)
    row    : int  - row to draw on     (0 = top edge)
    text   : str  - the string to draw
    fg     : int, optional - foreground colour (COLOR_* constant). Default WHITE.
    bg     : int, optional - background colour (COLOR_* constant). Default BLACK.
    attr   : int, optional - text style (ATTR_* flags, combine with |). Default NORMAL.
    scaled : bool, optional - if True, treat col/row as 720p reference coordinates
                              and scale them to actual terminal coords. Default False.
    """
    if scaled:
        col, row = scale(app, col, row)
    if row < 0 or row >= app.rows or col >= app.cols:
        return
    col  = max(0, col)
    text = text[:app.cols - col]
    _safe_addstr(app._win, row, col, text, _curses_attr(app, fg, bg, attr))


def draw_text_centered(app: App, row: int, text: str,
                       fg=COLOR_WHITE, bg=COLOR_BLACK, attr=ATTR_NORMAL) -> None:
    """
    Draw text centred horizontally on a given row.

    Args
    ----
    app  : App
    row  : int - which row to draw on (actual terminal coords)
    text : str - the string to centre and draw
    fg   : int, optional - foreground colour. Default WHITE.
    bg   : int, optional - background colour. Default BLACK.
    attr : int, optional - text style. Default NORMAL.
    """
    col = max(0, (app.cols - len(text)) // 2)
    draw_text(app, col, row, text, fg=fg, bg=bg, attr=attr)


def draw_multiline(app: App, col: int, row: int, text: str,
                   fg=COLOR_WHITE, bg=COLOR_BLACK, attr=ATTR_NORMAL,
                   max_width: int = None, max_height: int = None) -> int:
    """
    Draw a block of text with newline support and optional word-wrap.

    Args
    ----
    app        : App
    col        : int - left column
    row        : int - top row
    text       : str - text to draw; \\n line breaks are supported
    fg         : int, optional - foreground colour. Default WHITE.
    bg         : int, optional - background colour. Default BLACK.
    attr       : int, optional - text style. Default NORMAL.
    max_width  : int, optional - wrap lines longer than this. Defaults to app.cols - col.
    max_height : int, optional - stop after this many rows. Defaults to app.rows - row.

    Returns
    -------
    int - how many rows were actually drawn (handy for laying things out below)
    """
    if max_width  is None: max_width  = app.cols - col
    if max_height is None: max_height = app.rows - row

    lines_drawn = 0
    for raw_line in text.split("\n"):
        if lines_drawn >= max_height:
            break
        while len(raw_line) > max_width:
            break_at = raw_line.rfind(" ", 0, max_width)
            if break_at == -1:
                break_at = max_width
            draw_text(app, col, row + lines_drawn, raw_line[:break_at],
                      fg=fg, bg=bg, attr=attr)
            lines_drawn += 1
            raw_line = raw_line[break_at:].lstrip()
            if lines_drawn >= max_height:
                break
        if lines_drawn < max_height:
            draw_text(app, col, row + lines_drawn, raw_line, fg=fg, bg=bg, attr=attr)
            lines_drawn += 1

    return lines_drawn


# ---------------------------------------------------------------------------
# DRAWING - SHAPES AND BORDERS
# ---------------------------------------------------------------------------

def draw_box(app: App, col: int, row: int, width: int, height: int,
             title: str = "", fg=COLOR_WHITE, bg=COLOR_BLACK,
             double_border: bool = False, scaled: bool = False) -> None:
    """
    Draw a rectangular border using box-drawing characters.

    Think of it like a panel or frame. The interior is NOT cleared - call
    fill_region() first if you want a solid background inside the box.

    Single border uses  ┌─┐ │ └─┘
    Double border uses  ╔═╗ ║ ╚═╝

    Args
    ----
    app           : App
    col           : int  - left edge column
    row           : int  - top  edge row
    width         : int  - total width  INCLUDING the border columns
    height        : int  - total height INCLUDING the border rows
    title         : str, optional - text shown inside the top border
    fg            : int, optional - border colour. Default WHITE.
    bg            : int, optional - background colour. Default BLACK.
    double_border : bool, optional - use double-line box chars. Default False.
    scaled        : bool, optional - scale col/row from 720p reference. Default False.
    """
    if scaled:
        col, row = scale(app, col, row)

    B = _BOX_DOUBLE if double_border else _BOX_SINGLE
    a = _curses_attr(app, fg, bg, ATTR_NORMAL)

    top = B["tl"] + B["h"] * (width - 2) + B["tr"]
    if title:
        t = f" {title} "
        insert_at = max(1, (width - len(t)) // 2)
        top = top[:insert_at] + t + top[insert_at + len(t):]
        top = top[:width]
    _safe_addstr(app._win, row, col, top, a)

    for r in range(row + 1, row + height - 1):
        _safe_addstr(app._win, r, col,             B["v"], a)
        _safe_addstr(app._win, r, col + width - 1, B["v"], a)

    _safe_addstr(app._win, row + height - 1, col,
                 B["bl"] + B["h"] * (width - 2) + B["br"], a)


def draw_hline(app: App, col: int, row: int, length: int,
               char: str = "─", fg=COLOR_WHITE, bg=COLOR_BLACK) -> None:
    """
    Draw a horizontal line of a repeated character. Great for dividers.

    Args
    ----
    app    : App
    col    : int - starting column
    row    : int - row to draw on
    length : int - how many characters wide
    char   : str, optional - character to repeat. Default '─'.
    fg     : int, optional - foreground colour. Default WHITE.
    bg     : int, optional - background colour. Default BLACK.
    """
    _safe_addstr(app._win, row, col, char * length,
                 _curses_attr(app, fg, bg, ATTR_NORMAL))


def draw_vline(app: App, col: int, row: int, length: int,
               char: str = "│", fg=COLOR_WHITE, bg=COLOR_BLACK) -> None:
    """
    Draw a vertical line of a repeated character.

    Args
    ----
    app    : App
    col    : int - column to draw in
    row    : int - starting row
    length : int - how many characters tall
    char   : str, optional - character to repeat. Default '│'.
    fg     : int, optional - foreground colour. Default WHITE.
    bg     : int, optional - background colour. Default BLACK.
    """
    a = _curses_attr(app, fg, bg, ATTR_NORMAL)
    for r in range(row, row + length):
        _safe_addstr(app._win, r, col, char, a)


def fill_region(app: App, col: int, row: int, width: int, height: int,
                char: str = " ", fg=COLOR_WHITE, bg=COLOR_BLACK) -> None:
    """
    Fill a rectangular region with a single character (default: space).

    Use this to clear areas or paint a coloured background block.

    Args
    ----
    app    : App
    col    : int - left column
    row    : int - top row
    width  : int - width in columns
    height : int - height in rows
    char   : str, optional - fill character. Default space.
    fg     : int, optional - foreground colour. Default WHITE.
    bg     : int, optional - background colour. Default BLACK.
    """
    a    = _curses_attr(app, fg, bg, ATTR_NORMAL)
    line = (char * width)[:app.cols - col]
    for r in range(row, min(row + height, app.rows)):
        _safe_addstr(app._win, r, col, line, a)


def clear_screen(app: App) -> None:
    """
    Erase everything on screen.

    Note: run() already erases before each draw_fn call, so you normally
    do not need this. It is here if you need to erase mid-frame for some reason.

    Args
    ----
    app : App
    """
    if app._win:
        app._win.erase()


# ---------------------------------------------------------------------------
# DRAWING - WIDGETS
# ---------------------------------------------------------------------------

def draw_label(app: App, col: int, row: int, text: str,
               fg=COLOR_WHITE, bg=COLOR_BLACK, attr=ATTR_NORMAL,
               width: int = None) -> None:
    """
    Draw a static text label, optionally padded to a fixed width.

    If you give a width, the label is space-padded or truncated to fit
    exactly that many columns. Useful for aligned forms.

    Args
    ----
    app   : App
    col   : int - left column
    row   : int - row
    text  : str - label text
    fg    : int, optional - foreground colour. Default WHITE.
    bg    : int, optional - background colour. Default BLACK.
    attr  : int, optional - text style. Default NORMAL.
    width : int, optional - fixed column width. None means no padding.
    """
    if width is not None:
        text = text[:width].ljust(width)
    draw_text(app, col, row, text, fg=fg, bg=bg, attr=attr)


def draw_button(app: App, col: int, row: int, label: str,
                focused: bool = False,
                fg=COLOR_BLACK, bg=COLOR_WHITE,
                focus_fg=COLOR_BLACK, focus_bg=COLOR_YELLOW) -> int:
    """
    Draw a keyboard-focusable button that looks like  [ Label ].

    Your input_fn is responsible for detecting Enter when a button is focused
    and doing something with it. Pass focused=True for the currently selected
    button so the user can see where they are.

    Args
    ----
    app      : App
    col      : int  - left column
    row      : int  - row
    label    : str  - button text
    focused  : bool, optional - highlight this button. Default False.
    fg       : int,  optional - normal foreground. Default BLACK.
    bg       : int,  optional - normal background. Default WHITE.
    focus_fg : int,  optional - focused foreground. Default BLACK.
    focus_bg : int,  optional - focused background. Default YELLOW.

    Returns
    -------
    int - the rendered width of the button in columns, useful for positioning
          the next button alongside it
    """
    text = f"[ {label} ]"
    draw_text(app, col, row, text,
              fg=focus_fg if focused else fg,
              bg=focus_bg if focused else bg,
              attr=ATTR_BOLD if focused else ATTR_NORMAL)
    return len(text)


def draw_checkbox(app: App, col: int, row: int, label: str,
                  checked: bool = False, focused: bool = False,
                  fg=COLOR_WHITE, bg=COLOR_BLACK) -> None:
    """
    Draw a checkbox.  Renders as  [X] Label  or  [ ] Label.

    Your app is responsible for toggling the checked state when the user
    presses Enter or Space on this item.

    Args
    ----
    app     : App
    col     : int  - left column
    row     : int  - row
    label   : str  - text shown next to the checkbox
    checked : bool, optional - whether it is ticked. Default False.
    focused : bool, optional - whether it is focused (bold highlight). Default False.
    fg      : int,  optional - foreground colour. Default WHITE.
    bg      : int,  optional - background colour. Default BLACK.
    """
    draw_text(app, col, row, f"[{'X' if checked else ' '}] {label}",
              fg=fg, bg=bg, attr=ATTR_BOLD if focused else ATTR_NORMAL)


def draw_radio(app: App, col: int, row: int, label: str,
               selected: bool = False, focused: bool = False,
               fg=COLOR_WHITE, bg=COLOR_BLACK) -> None:
    """
    Draw a radio button.  Renders as  (o) Label  or  ( ) Label.

    Args
    ----
    app      : App
    col      : int  - left column
    row      : int  - row
    label    : str  - text shown next to the radio button
    selected : bool, optional - whether this option is selected. Default False.
    focused  : bool, optional - whether it is focused (bold highlight). Default False.
    fg       : int,  optional - foreground colour. Default WHITE.
    bg       : int,  optional - background colour. Default BLACK.
    """
    draw_text(app, col, row, f"({'o' if selected else ' '}) {label}",
              fg=fg, bg=bg, attr=ATTR_BOLD if focused else ATTR_NORMAL)


def draw_input(app: App, col: int, row: int, width: int, value: str,
               focused: bool = False, password: bool = False,
               fg=COLOR_WHITE, bg=COLOR_BLACK,
               focus_fg=COLOR_WHITE, focus_bg=COLOR_BLUE,
               placeholder: str = "") -> None:
    """
    Draw a single-line text input field.

    This widget does NOT handle keyboard input itself. Your input_fn must
    update the value string and pass it back in each frame. See the todo
    app example in the docs for how to wire this up.

    There is no visible cursor. AstralixiOS TUI apps do not support a text
    cursor. The focused field is highlighted instead so the user knows where
    they are typing.

    Args
    ----
    app         : App
    col         : int  - left column
    row         : int  - row
    width       : int  - field width in columns (includes the [ ] brackets)
    value       : str  - current text content
    focused     : bool, optional - highlight the field when True. Default False.
    password    : bool, optional - show asterisks instead of actual text. Default False.
    fg          : int,  optional - normal foreground. Default WHITE.
    bg          : int,  optional - normal background. Default BLACK.
    focus_fg    : int,  optional - focused foreground. Default WHITE.
    focus_bg    : int,  optional - focused background. Default BLUE.
    placeholder : str,  optional - dim hint text shown when value is empty. Default "".
    """
    inner = width - 2   # inner width excluding the [ ] brackets

    if not value and placeholder and not focused:
        display = placeholder[:inner].ljust(inner)
        a = _curses_attr(app, COLOR_WHITE, focus_bg if focused else bg, ATTR_DIM)
    else:
        display_val = ("*" * len(value)) if password else value
        # Show the tail end of the string if it overflows the field
        display = display_val[-inner:].ljust(inner) if len(display_val) >= inner \
                  else display_val.ljust(inner)
        a = _curses_attr(app,
                         focus_fg if focused else fg,
                         focus_bg if focused else bg,
                         ATTR_NORMAL)

    _safe_addstr(app._win, row, col, f"[{display}]", a)


def draw_list(app: App, col: int, row: int, width: int, height: int,
              items: list, selected: int = 0, scroll: int = 0,
              fg=COLOR_WHITE, bg=COLOR_BLACK,
              sel_fg=COLOR_BLACK, sel_bg=COLOR_CYAN) -> None:
    """
    Draw a scrollable list of items.

    Your input_fn handles moving the selection and updating the scroll
    offset. See the quick reference for the scroll-clamping pattern.

    Args
    ----
    app      : App
    col      : int  - left column
    row      : int  - top row
    width    : int  - width in columns
    height   : int  - number of visible rows
    items    : list[str] - the items to display
    selected : int, optional - index of the highlighted item. Default 0.
    scroll   : int, optional - index of the first visible item. Default 0.
    fg       : int, optional - normal item foreground. Default WHITE.
    bg       : int, optional - normal item background. Default BLACK.
    sel_fg   : int, optional - selected item foreground. Default BLACK.
    sel_bg   : int, optional - selected item background. Default CYAN.
    """
    for i in range(height):
        idx = scroll + i
        r   = row + i
        if idx >= len(items):
            fill_region(app, col, r, width, 1, bg=bg)
            continue
        text = str(items[idx])[:width].ljust(width)
        if idx == selected:
            draw_text(app, col, r, text, fg=sel_fg, bg=sel_bg, attr=ATTR_BOLD)
        else:
            draw_text(app, col, r, text, fg=fg, bg=bg)


def draw_progress(app: App, col: int, row: int, width: int,
                  value: float, max_value: float = 100.0,
                  fg=COLOR_GREEN, bg=COLOR_BLACK,
                  show_pct: bool = True) -> None:
    """
    Draw a horizontal progress bar.

    Looks like:  [████████░░░░░░░░░░░░] 40%

    Args
    ----
    app       : App
    col       : int   - left column
    row       : int   - row
    width     : int   - total width including the [ ] brackets
    value     : float - current value
    max_value : float, optional - value that represents 100%. Default 100.0.
    fg        : int,  optional - filled portion colour. Default GREEN.
    bg        : int,  optional - background colour. Default BLACK.
    show_pct  : bool, optional - overlay the percentage as text. Default True.
    """
    inner  = width - 2
    pct    = max(0.0, min(1.0, value / max_value)) if max_value else 0.0
    filled = int(pct * inner)
    empty  = inner - filled

    _safe_addstr(app._win, row, col,                "[",           _curses_attr(app, COLOR_WHITE, bg, ATTR_NORMAL))
    _safe_addstr(app._win, row, col + 1,            "█" * filled,  _curses_attr(app, fg,          bg, ATTR_NORMAL))
    _safe_addstr(app._win, row, col + 1 + filled,   "░" * empty,   _curses_attr(app, COLOR_BLACK, bg, ATTR_NORMAL))
    _safe_addstr(app._win, row, col + 1 + inner,    "]",           _curses_attr(app, COLOR_WHITE, bg, ATTR_NORMAL))

    if show_pct:
        pct_str = f"{int(pct * 100):3d}%"
        draw_text(app, col + max(0, (width - len(pct_str)) // 2), row,
                  pct_str, fg=COLOR_WHITE, bg=bg, attr=ATTR_BOLD)


def draw_scrollbar(app: App, col: int, row: int, height: int,
                   total: int, visible: int, scroll: int,
                   fg=COLOR_WHITE, bg=COLOR_BLACK) -> None:
    """
    Draw a vertical scrollbar track with a thumb showing scroll position.

    Pair this with draw_list() or draw_multiline() to give a visual hint.
    The scrollbar does not handle input on its own.

    Args
    ----
    app     : App
    col     : int - column to draw in
    row     : int - top row
    height  : int - height of the scrollbar track in rows
    total   : int - total number of items or lines
    visible : int - number currently visible on screen
    scroll  : int - current scroll offset (index of first visible item)
    fg      : int, optional - thumb colour. Default WHITE.
    bg      : int, optional - track colour. Default BLACK.
    """
    a_track = _curses_attr(app, bg, bg, ATTR_NORMAL)
    a_thumb = _curses_attr(app, fg, fg, ATTR_REVERSE)

    if total <= visible:
        for r in range(height):
            _safe_addstr(app._win, row + r, col, "│", a_track)
        return

    thumb_size = max(1, int(height * visible / total))
    thumb_pos  = int((height - thumb_size) * scroll / max(1, total - visible))

    for r in range(height):
        is_thumb = thumb_pos <= r < thumb_pos + thumb_size
        _safe_addstr(app._win, row + r, col,
                     "█" if is_thumb else "│",
                     a_thumb if is_thumb else a_track)


def draw_menu_bar(app: App, items: list,
                  fg=COLOR_BLACK, bg=COLOR_WHITE,
                  sel_fg=COLOR_WHITE, sel_bg=COLOR_BLUE,
                  selected: int = -1) -> None:
    """
    Draw a horizontal menu bar across row 0 of the screen.

    Args
    ----
    app      : App
    items    : list[str] - menu item labels, e.g. ["File", "Edit", "Help"]
    fg       : int, optional - normal text colour. Default BLACK.
    bg       : int, optional - bar background. Default WHITE.
    sel_fg   : int, optional - highlighted item text. Default WHITE.
    sel_bg   : int, optional - highlighted item background. Default BLUE.
    selected : int, optional - index of the open or highlighted menu (-1 = none).
    """
    fill_region(app, 0, 0, app.cols, 1, bg=bg)
    cursor = 0
    for i, item in enumerate(items):
        label = f" {item} "
        if i == selected:
            draw_text(app, cursor, 0, label, fg=sel_fg, bg=sel_bg, attr=ATTR_BOLD)
        else:
            draw_text(app, cursor, 0, label, fg=fg, bg=bg)
        cursor += len(label)


def draw_status_bar(app: App, text: str,
                    fg=COLOR_BLACK, bg=COLOR_WHITE,
                    right_text: str = "") -> None:
    """
    Draw a status bar across the very bottom row of the screen.

    A good place to show key bindings. Always include your exit shortcut
    here so users can see how to quit the app.

    Args
    ----
    app        : App
    text       : str - left-aligned message, e.g. " Q Quit  Arrow keys Navigate"
    fg         : int, optional - foreground colour. Default BLACK.
    bg         : int, optional - background colour. Default WHITE.
    right_text : str, optional - right-aligned text, e.g. "Ln 4, Col 12".
    """
    row = app.rows - 1
    fill_region(app, 0, row, app.cols, 1, bg=bg)
    draw_text(app, 1, row, text[:app.cols - 2], fg=fg, bg=bg)
    if right_text:
        draw_text(app, max(0, app.cols - len(right_text) - 1), row,
                  right_text, fg=fg, bg=bg)


def draw_table(app: App, col: int, row: int, width: int, height: int,
               headers: list, rows: list,
               col_widths: list = None,
               selected_row: int = -1,
               fg=COLOR_WHITE, bg=COLOR_BLACK,
               header_fg=COLOR_BLACK, header_bg=COLOR_CYAN,
               sel_fg=COLOR_BLACK, sel_bg=COLOR_YELLOW) -> None:
    """
    Draw a simple data table with column headers.

    Args
    ----
    app          : App
    col          : int        - left column
    row          : int        - top row
    width        : int        - total width in columns
    height       : int        - number of data rows visible (not counting the header row)
    headers      : list[str]  - column header labels
    rows         : list[list] - data rows, each a list of values (auto-converted to str)
    col_widths   : list[int], optional - width of each column in chars. Divided evenly if None.
    selected_row : int, optional - 0-based index of the highlighted row. -1 = none.
    fg           : int, optional - data cell foreground. Default WHITE.
    bg           : int, optional - data cell background. Default BLACK.
    header_fg    : int, optional - header foreground. Default BLACK.
    header_bg    : int, optional - header background. Default CYAN.
    sel_fg       : int, optional - selected row foreground. Default BLACK.
    sel_bg       : int, optional - selected row background. Default YELLOW.
    """
    n_cols = len(headers)
    if col_widths is None:
        base  = width // n_cols
        extra = width % n_cols
        col_widths = [base + (1 if i < extra else 0) for i in range(n_cols)]

    c = col
    for i, h in enumerate(headers):
        draw_text(app, c, row, str(h)[:col_widths[i]].ljust(col_widths[i]),
                  fg=header_fg, bg=header_bg, attr=ATTR_BOLD)
        c += col_widths[i]

    for ri, data_row in enumerate(rows[:height]):
        r   = row + 1 + ri
        c   = col
        sel = (ri == selected_row)
        for ci, cell_val in enumerate(data_row[:n_cols]):
            draw_text(app, c, r, str(cell_val)[:col_widths[ci]].ljust(col_widths[ci]),
                      fg=sel_fg if sel else fg,
                      bg=sel_bg if sel else bg)
            c += col_widths[ci]


def draw_modal(app: App, title: str, body: str, buttons: list,
               focused_btn: int = 0,
               width: int = None, height: int = None) -> None:
    """
    Draw a centred dialog box over the existing screen content.

    Your app controls when the modal appears and disappears. Just stop
    calling this function once the user has made a choice, and the modal
    will disappear on the next frame.

    Args
    ----
    app         : App
    title       : str       - title text shown in the top border
    body        : str       - message body; \\n line breaks are supported
    buttons     : list[str] - button labels, e.g. ["OK", "Cancel"]
    focused_btn : int, optional - index of the focused button. Default 0.
    width       : int, optional - dialog width. Defaults to min(60, app.cols-4).
    height      : int, optional - dialog height. Auto-calculated from body if None.
    """
    if width is None: width = min(60, app.cols - 4)
    body_lines = body.split("\n")
    if height is None: height = len(body_lines) + 4   # top/bottom borders + button row

    dlg_col = (app.cols  - width)  // 2
    dlg_row = (app.rows  - height) // 2

    fill_region(app, dlg_col, dlg_row, width, height, bg=COLOR_WHITE)
    draw_box(app, dlg_col, dlg_row, width, height,
             title=title, fg=COLOR_BLUE, bg=COLOR_WHITE, double_border=True)

    for i, line in enumerate(body_lines[:height - 3]):
        draw_text(app, dlg_col + 2, dlg_row + 1 + i,
                  line[:width - 4], fg=COLOR_BLACK, bg=COLOR_WHITE)

    btn_row = dlg_row + height - 2
    total_w = sum(len(b) + 4 for b in buttons) + (len(buttons) - 1)
    btn_col = dlg_col + (width - total_w) // 2
    for i, btn in enumerate(buttons):
        bw = draw_button(app, btn_col, btn_row, btn, focused=(i == focused_btn),
                         fg=COLOR_BLACK, bg=COLOR_WHITE,
                         focus_fg=COLOR_WHITE, focus_bg=COLOR_BLUE)
        btn_col += bw + 1


def draw_spinner(app: App, col: int, row: int, frame: int,
                 style: str = "braille",
                 fg=COLOR_CYAN, bg=COLOR_BLACK) -> None:
    """
    Draw an animated single-character spinner. Good for showing that
    something is loading or working in the background.

    To animate it, just pass a frame counter that increases over time.
    The easiest way:  frame = int(time.time() * 10)

    Styles:
        "braille"  ->  cycles through 8 braille dot patterns (default)
        "dots"     ->  cycles through 10 braille dot patterns
        "line"     ->  | / - \\
        "box"      ->  ◰ ◳ ◲ ◱

    Args
    ----
    app   : App
    col   : int - column
    row   : int - row
    frame : int - current animation frame index (wraps automatically)
    style : str, optional - spinner style name. Default "braille".
    fg    : int, optional - spinner colour. Default CYAN.
    bg    : int, optional - background colour. Default BLACK.
    """
    frames = _SPINNERS.get(style, _SPINNERS["braille"])
    draw_text(app, col, row, frames[frame % len(frames)], fg=fg, bg=bg, attr=ATTR_BOLD)


# ---------------------------------------------------------------------------
# INPUT HELPERS
# ---------------------------------------------------------------------------

def key_name(key: int) -> str:
    """
    Convert a raw curses key code to a readable string name.

    Use this in your input_fn to handle keys by name rather than raw number.

    Args
    ----
    key : int - key code received in your input_fn

    Returns
    -------
    str - e.g. "UP", "DOWN", "ENTER", "ESC", "BACKSPACE", "TAB", "a", "A", "F1"

    Example
    -------
        def on_key(app, key):
            name = key_name(key)
            if name == "ENTER":
                confirm()
            elif name == "UP":
                move_selection_up()
    """
    if key in _KEY_NAMES:
        return _KEY_NAMES[key]
    if 0 <= key < 256:
        return chr(key)
    return f"KEY_{key}"


def is_printable(key: int) -> bool:
    """
    Return True if the key code is a standard printable ASCII character
    (space through ~, i.e. codes 32 to 126).

    Use this to decide whether a keypress should be appended to a text field.

    Args
    ----
    key : int - key code from your input_fn

    Returns
    -------
    bool

    Example
    -------
        if is_printable(key):
            my_text += chr(key)
    """
    return 32 <= key <= 126


# ---------------------------------------------------------------------------
# COLOUR HELPERS
# ---------------------------------------------------------------------------

def set_color_pair(pair_id: int, fg: int, bg: int) -> None:
    """
    Define a custom colour pair by ID for use with color_attr().

    The API pre-defines pairs 1-64 internally for the 8x8 standard colour
    grid. Use IDs outside that range (up to 255 if your terminal supports it)
    for your own custom pairs.

    Args
    ----
    pair_id : int - colour pair ID (1-63 safe range)
    fg      : int - foreground colour (COLOR_* constant)
    bg      : int - background colour (COLOR_* constant)
    """
    try:
        curses.init_pair(max(1, min(pair_id, 63)), fg, bg)
    except Exception:
        pass


def color_attr(pair_id: int, attr: int = ATTR_NORMAL) -> int:
    """
    Combine a colour pair ID with text attribute flags into a single value
    you can pass to the `attr` parameter of any draw_* function.

    Most useful when you have set up a custom pair with set_color_pair().

    Args
    ----
    pair_id : int - colour pair ID from set_color_pair()
    attr    : int, optional - ATTR_* flags to combine. Default ATTR_NORMAL.

    Returns
    -------
    int - the combined curses attribute value

    Example
    -------
        set_color_pair(10, COLOR_RED, COLOR_YELLOW)
        draw_text(app, 2, 4, "Alert!", attr=color_attr(10, ATTR_BOLD))
    """
    result = curses.color_pair(pair_id)
    if attr & ATTR_BOLD:      result |= curses.A_BOLD
    if attr & ATTR_DIM:       result |= curses.A_DIM
    if attr & ATTR_UNDERLINE: result |= curses.A_UNDERLINE
    if attr & ATTR_BLINK:     result |= curses.A_BLINK
    if attr & ATTR_REVERSE:   result |= curses.A_REVERSE
    return result


# ---------------------------------------------------------------------------
# UTILITY
# ---------------------------------------------------------------------------

def get_terminal_info() -> dict:
    """
    Return a dict of information about the current terminal environment.

    Useful for debugging or for making decisions based on terminal capabilities.

    Returns
    -------
    dict with keys:
        "cols"    : int   - terminal width in columns
        "rows"    : int   - terminal height in rows
        "colors"  : int   - number of supported colours (usually 8 or 256)
        "term"    : str   - the TERM environment variable value
        "scale_x" : float - horizontal scale factor vs. 720p reference grid
        "scale_y" : float - vertical   scale factor vs. 720p reference grid
    """
    size = shutil.get_terminal_size(fallback=(REF_COLS, REF_ROWS))
    try:
        colors = curses.COLORS
    except Exception:
        colors = 8
    return {
        "cols":    size.columns,
        "rows":    size.lines,
        "colors":  colors,
        "term":    os.environ.get("TERM", "unknown"),
        "scale_x": size.columns / REF_COLS,
        "scale_y": size.lines   / REF_ROWS,
    }


def show_notification(app: App, message: str, duration: float = 2.0,
                      fg=COLOR_BLACK, bg=COLOR_YELLOW) -> None:
    """
    Show a brief notification banner at the bottom of the screen that
    disappears automatically after a set time. Non-blocking.

    If multiple notifications are queued, the most recent one is shown.

    Args
    ----
    app      : App
    message  : str   - text to show
    duration : float, optional - seconds until auto-dismiss. Default 2.0.
    fg       : int,  optional - foreground colour. Default BLACK.
    bg       : int,  optional - background colour. Default YELLOW.
    """
    app._notifications.append((message, time.monotonic() + duration, fg, bg))


def set_title(app: App, title: str) -> None:
    """
    Update the app title stored in app.title.

    This does not draw anything on its own. If your draw_fn uses app.title
    (e.g. as the title argument to draw_box()), it will show the new value
    on the next frame.

    Args
    ----
    app   : App
    title : str - new title string
    """
    app.title = title


def sleep(seconds: float) -> None:
    """
    Pause for a given duration using curses timing.

    Use this instead of time.sleep() inside draw or input functions,
    because it works correctly with the curses event loop.

    Args
    ----
    seconds : float - how long to pause (fractions are fine, e.g. 0.05)
    """
    curses.napms(int(seconds * 1000))
