# Making TUI Apps for AstralixiOS

So you want to make an app for AstralixiOS. Good. This guide covers everything from writing your first line of code to getting it into the official app library.

---

## What even is a TUI app?

TUI stands for **Terminal User Interface**. It is basically an advanced CLI with very basic graphics. Think coloured text, boxes, borders, lists, and buttons, all drawn inside the terminal using characters. No windows. No GUI. Just the terminal.

Apps like `htop`, `nano`, and `vim` are all TUI apps. That is the kind of thing you are making.

Some ideas for what you could build: file managers, music players, task trackers, calculators, note apps, chat clients, text editors, games. If it can be done in a terminal, it counts.

---

## The Rules

These are not suggestions. If your app breaks any of these, it will not be accepted.

**1. TUI only. No GUI.**
Your app lives inside the terminal. Do not use `tkinter` to open a window. Do not use `turtle`, `pygame`, or anything that draws outside the terminal. If it opens a separate window, it gets rejected.

**2. Python Standard Library only.**
No `pip install`. No `requirements.txt`. No third-party packages at all. Everything your app needs has to come from the Python Standard Library that ships with CPython. If someone downloads your app and runs it with a clean Python install, it must just work.

**3. Use the AstralixiOS API.**
Your app must import `astralixios_api` and use it. Do not write raw curses code. Do not call `os.system("clear")`. Do not print ANSI escape codes manually. The API handles all of that for you.

**4. One file only.**
Your entire app must be a single `.py` file. No packages, no folders of modules, no imports from other files you wrote. Just the one file. This will probably change in a future version, but for now it is a hard requirement.

**5. Your app must have a way to exit.**
Q and Escape quit any app automatically. But you need to make this obvious to the user. Put it in the status bar. If a reviewer opens your app and cannot figure out how to leave, it will be rejected.

---

## Setting Up

Grab `astralixios_api.py` from the AstralixiOS SDK and put it in the same folder as your app file.

```
my_app/
├── astralixios_api.py   <- the API
└── my_app.py            <- your app
```

That is it. Your app imports from `astralixios_api` and the OS takes care of the rest when it is installed.

---

## Your First App

Every AstralixiOS app follows the same basic structure:

1. Create an `App` object
2. Write a draw function (gets called every frame)
3. Call `run()` to start

Here is the smallest valid app:

```python
from astralixios_api import App, draw_box, draw_text, draw_status_bar, run

def draw(app):
    draw_box(app, 0, 0, app.cols, app.rows, title="Hello World", double_border=True)
    draw_text(app, 4, 2, "Welcome to AstralixiOS!", attr=1)
    draw_status_bar(app, " Q to quit")

app = App(title="Hello World")
run(app, draw)
```

Run it with:

```
python my_app.py
```

Press **Q** or **Escape** to quit.

---

## The App Object

`App` is the thing you pass to every API call. Make one at the start and use it everywhere.

```python
app = App(title="My App", fps=30)
```

After `run()` starts, these attributes are available:

| Attribute | Type | What it is |
|-----------|------|------------|
| `app.cols` | int | Terminal width in columns |
| `app.rows` | int | Terminal height in rows |
| `app.scale_x` | float | Horizontal scale vs. the 720p reference |
| `app.scale_y` | float | Vertical scale vs. the 720p reference |
| `app.title` | str | App title, change it with `set_title()` |
| `app.running` | bool | False once `quit()` is called |

`app.cols` and `app.rows` are measured once when you create the `App` object and stay fixed. AstralixiOS runs on a fixed 720p display and does not resize at runtime.

---

## Screen Coordinates

The terminal is a grid of character cells. Top-left is `(col=0, row=0)`. Columns go right, rows go down.

```
(0,0) ────────────────────── (cols-1, 0)
  |                                  |
  |         your app here            |
  |                                  |
(0, rows-1) ─────────── (cols-1, rows-1)
```

### The 720p Reference Grid

AstralixiOS targets a 720p display, which maps to roughly **213 columns x 45 rows** at 6x16 pixels per character cell. Use `app.cols` and `app.rows` for all your layout calculations so things fit correctly.

If you want to design positions based on the 720p reference and then scale them to the actual terminal, use `scale()`:

```python
col, row = scale(app, 106, 22)   # roughly the centre of a 720p screen
```

Or just use `app.cols` and `app.rows` directly, which is usually simpler:

```python
# A centred panel that always fits
panel_w = min(60, app.cols - 4)
panel_h = min(20, app.rows - 4)
panel_col = (app.cols - panel_w) // 2
panel_row = (app.rows - panel_h) // 2
draw_box(app, panel_col, panel_row, panel_w, panel_h)
```

Never hardcode column or row numbers. Always base them on `app.cols` and `app.rows`.

---

## The Draw Function

Your draw function is called once per frame (30 fps by default). Everything the user sees must be drawn here, every single frame. The screen is wiped clean before each call, so you always start fresh.

```python
def draw(app):
    draw_box(app, 0, 0, app.cols, app.rows)
    draw_text(app, 2, 2, f"Counter: {counter}")
```

Do not try to keep things on screen between frames. Just draw everything you need each time.

Keep it fast. File I/O, network calls, and heavy computation inside `draw_fn` will make your app feel sluggish. If you need to do background work, use `threading.Thread` and share the result through a simple variable.

---

## Handling Input

Pass an `input_fn` to `run()`. It gets called on every keypress with the raw key code.

```python
def on_key(app, key):
    name = key_name(key)
    if name == "ENTER":
        do_something()
    elif name == "UP":
        move_up()
    elif is_printable(key):
        text_buffer += chr(key)

run(app, draw, on_key)
```

Common key names from `key_name()`:

| Key | String |
|-----|--------|
| Arrow keys | `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"` |
| Enter | `"ENTER"` |
| Backspace | `"BACKSPACE"` |
| Delete | `"DELETE"` |
| Tab | `"TAB"` |
| Escape | `"ESC"` |
| F1 to F12 | `"F1"` through `"F12"` |
| Printable chars | `"a"`, `"B"`, `"1"`, `"!"`, etc. |

### A note on mouse support

There is no mouse support right now. It will be added in a future version of AstralixiOS, and when it is, it will only be for scrolling. No clicking, no cursor positioning. Design your app to be fully keyboard-driven.

---

## Drawing Reference

### Colours

```python
COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW
COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
```

### Text Attributes

Combine them with `|`:

```python
ATTR_NORMAL     # plain
ATTR_BOLD       # bold or bright
ATTR_DIM        # dim or faint
ATTR_UNDERLINE  # underlined
ATTR_BLINK      # blinking (not all terminals support this)
ATTR_REVERSE    # swap fg and bg
ATTR_ITALIC     # italic (not all terminals support this)

# Example
draw_text(app, 2, 3, "Warning!", fg=COLOR_RED, attr=ATTR_BOLD | ATTR_UNDERLINE)
```

### Text

```python
draw_text(app, col, row, "Hello!")                      # basic text
draw_text_centered(app, row, "Centred heading")         # auto-centred on the row
draw_multiline(app, col, row, "Line 1\nLine 2\n...")    # newlines and word wrap
draw_label(app, col, row, "Name:", width=12)            # fixed-width label
```

### Borders and Shapes

```python
draw_box(app, col, row, width, height)                      # single border box
draw_box(app, col, row, width, height, double_border=True)  # double border
draw_box(app, col, row, width, height, title="Panel")       # box with a title
draw_hline(app, col, row, length)                           # horizontal line
draw_vline(app, col, row, length)                           # vertical line
fill_region(app, col, row, width, height, bg=COLOR_BLUE)    # filled rectangle
clear_screen(app)                                           # erase everything
```

### Widgets

```python
# Button - returns its width so you can position the next one
width = draw_button(app, col, row, "OK", focused=True)

# Checkbox
draw_checkbox(app, col, row, "Enable thing", checked=True, focused=True)

# Radio button
draw_radio(app, col, row, "Option A", selected=True)

# Text input field (you handle the keyboard logic yourself)
draw_input(app, col, row, width=30, value=text_buf, focused=True,
           placeholder="Type here...")

# Scrollable list
draw_list(app, col, row, width=40, height=10,
          items=my_list, selected=cursor, scroll=offset)

# Progress bar
draw_progress(app, col, row, width=40, value=75, max_value=100)

# Scrollbar (pair this with draw_list to show scroll position)
draw_scrollbar(app, col, row, height=10,
               total=len(items), visible=10, scroll=offset)

# Menu bar (draws across row 0)
draw_menu_bar(app, ["File", "Edit", "View", "Help"], selected=0)

# Status bar (draws at the bottom row - always put your exit shortcut here)
draw_status_bar(app, " Q Quit  Arrows Navigate", right_text="Ln 1, Col 1")

# Data table
draw_table(app, col, row, width=60, height=8,
           headers=["Name", "Size", "Date"],
           rows=[["file.txt", "12 KB", "2026-05-01"]])

# Modal dialog
draw_modal(app, title="Confirm", body="Delete this file?",
           buttons=["Yes", "No"], focused_btn=1)

# Animated spinner (pass an increasing frame number to animate it)
import time
draw_spinner(app, col, row, frame=int(time.time() * 10), style="braille")
```

---

## A Full Example: Todo App

```python
from astralixios_api import (
    App, run, quit,
    draw_box, draw_text, draw_list, draw_status_bar,
    draw_input, draw_button, draw_modal,
    key_name, is_printable,
    COLOR_WHITE, COLOR_BLACK,
    ATTR_BOLD,
)

todos   = ["Buy groceries", "Write AstralixiOS app", "Read the docs"]
cursor  = 0
scroll  = 0
adding  = False
new_text = ""
confirm_delete = False


def draw(app):
    global scroll

    list_h = app.rows - 6

    draw_box(app, 0, 0, app.cols, app.rows, title="Todo", double_border=True)

    # Clamp the scroll offset so the selected item is always visible
    if cursor < scroll:
        scroll = cursor
    if cursor >= scroll + list_h:
        scroll = cursor - list_h + 1

    draw_list(app, 2, 2, app.cols - 4, list_h,
              items=todos, selected=cursor, scroll=scroll)

    if adding:
        draw_text(app, 2, app.rows - 4, "New todo:")
        draw_input(app, 12, app.rows - 4, app.cols - 14,
                   value=new_text, focused=True)

    draw_button(app, 2,  app.rows - 2, "Add",    focused=not adding)
    draw_button(app, 10, app.rows - 2, "Delete", focused=False)
    draw_status_bar(app, " Arrows Navigate  A Add  D Delete  Q Quit")

    if confirm_delete:
        draw_modal(app, "Confirm", "Delete this todo?",
                   ["Yes", "No"], focused_btn=1)


def on_key(app, key):
    global cursor, scroll, adding, new_text, confirm_delete

    name = key_name(key)

    if confirm_delete:
        if name == "ENTER":
            if cursor < len(todos):
                todos.pop(cursor)
                cursor = max(0, cursor - 1)
            confirm_delete = False
        elif name == "ESC":
            confirm_delete = False
        return

    if adding:
        if name == "ENTER":
            if new_text.strip():
                todos.append(new_text.strip())
                cursor = len(todos) - 1
            new_text = ""
            adding   = False
        elif name == "ESC":
            new_text = ""
            adding   = False
        elif name == "BACKSPACE":
            new_text = new_text[:-1]
        elif is_printable(key):
            new_text += chr(key)
        return

    if name == "UP":
        cursor = max(0, cursor - 1)
    elif name == "DOWN":
        cursor = min(len(todos) - 1, cursor + 1)
    elif name in ("a", "A"):
        adding = True
    elif name in ("d", "D"):
        if todos:
            confirm_delete = True


app = App(title="Todo")
run(app, draw, on_key)
```

---

## Converting Your App

Once your app is working, convert it into an `.axapp` bundle so it can be installed from the app library.

Inside AstralixiOS, run:

```
axpack my_app.py
```

This packs your script into a signed bundle. Then install and test it locally:

```
axinstall my_app.axapp
```

After that it shows up in the app list like any other installed app.

---

## Submitting to the Official Library

Anyone can submit an app. Here is how it works:

### Step 1 - Set up your repo

Create a public GitHub repository for your app. It needs:

```
my_app/
├── my_app.py     <- your app (single file, nothing else you wrote)
├── README.md     <- what the app does and how to use it
└── LICENSE       <- any open source licence you like
```

Do NOT include `astralixios_api.py` in your repo. The OS provides it.

### Step 2 - Open a pull request

Fork the AstralixiOS App Library repo and open a pull request adding your app to the `submissions/` folder. Include an entry in `submissions/index.json` with your app name, description, and category.

### Step 3 - Manual review

A maintainer will check your app for:

- Proper TUI only (no GUI, no external windows)
- Python Standard Library only (no third-party packages)
- Uses `astralixios_api` correctly
- Has a visible exit option
- README is clear and makes sense
- The app actually works

Reviews are done manually right now. There are no automated checks. If something needs fixing you will get a comment on the PR explaining what to change. Push a fix and the review continues.

Apps that pass get merged and show up in the library within 24 hours.

---

## Tips

**Keyboard-only navigation.** Your app must be fully usable without a mouse. Use arrow keys to move, Tab to cycle focus, and Enter to confirm. If something is only reachable by clicking, it is not accessible.

**Always show the controls.** Use `draw_status_bar()` at the bottom to show the keybindings for the current screen. Always include the exit shortcut. It is a requirement, and users will thank you for it.

**Build everything from `app.cols` and `app.rows`.** Never hardcode positions. Basing your layout on `app.cols` and `app.rows` means it will look correct at the actual terminal size.

**Keep `draw_fn` fast.** It runs 30 times a second. Anything slow in there will make the whole app feel laggy. Move background work to a thread and share results through a variable.

**Test at 80x24.** That is a very common terminal default. Make sure nothing breaks or overflows at that size.

---

## API Quick Reference

| Function | What it does |
|----------|--------------|
| `App(title, fps)` | Create the app object |
| `run(app, draw_fn, input_fn)` | Start the main loop |
| `quit(app)` | Exit the loop |
| `get_size(app)` | Returns `(cols, rows)` |
| `scale(app, ref_col, ref_row)` | Convert 720p coords to actual terminal coords |
| `draw_text(...)` | Draw a string |
| `draw_text_centered(...)` | Draw a centred string |
| `draw_multiline(...)` | Draw word-wrapped text |
| `draw_label(...)` | Fixed-width label |
| `draw_box(...)` | Rectangular border |
| `draw_hline(...)` | Horizontal line |
| `draw_vline(...)` | Vertical line |
| `fill_region(...)` | Filled rectangle |
| `clear_screen(app)` | Erase the screen |
| `draw_button(...)` | Keyboard-focusable button |
| `draw_checkbox(...)` | Checkbox |
| `draw_radio(...)` | Radio button |
| `draw_input(...)` | Text input field |
| `draw_list(...)` | Scrollable list |
| `draw_progress(...)` | Progress bar |
| `draw_scrollbar(...)` | Vertical scroll indicator |
| `draw_menu_bar(...)` | Top menu bar |
| `draw_status_bar(...)` | Bottom status bar |
| `draw_table(...)` | Data table with headers |
| `draw_modal(...)` | Centred dialog box |
| `draw_spinner(...)` | Animated spinner character |
| `key_name(key)` | Key code to readable string |
| `is_printable(key)` | True if key is a printable char |
| `set_color_pair(id, fg, bg)` | Define a custom colour pair |
| `color_attr(id, attr)` | Build a combined attribute int |
| `get_terminal_info()` | Dict of terminal properties |
| `show_notification(...)` | Timed bottom banner |
| `set_title(app, title)` | Update the app title |
| `sleep(seconds)` | Non-blocking pause |

---

*AstralixiOS Developer Documentation - draft v0.1*
