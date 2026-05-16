# Making Apps for AstralixiOS

So you wanna make an app for AstralixiOS. Sick. This guide walks you through everything — from writing your very first line of code all the way to getting it into the official app library. You don't need to be a programming genius to follow this. If you know basic Python, you're good.

---

## First — What even IS AstralixiOS?

Okay so quick thing before we get into code. AstralixiOS is NOT a custom operating system or kernel or anything crazy like that. It's basically just a program — called the Astralixi binary — that runs on top of regular Raspberry Pi OS Lite. When the device boots up, a startup script launches the Astralixi binary automatically, and boom, you've got the full Astralixi experience.

Your app runs inside that environment. It's literally just a Python script that uses the AstralixiOS API. That's it. Nothing wild going on under the hood.

---

## What's a TUI App?

TUI stands for **Terminal User Interface**. Basically, imagine a program that runs inside a terminal (like the black command line screen) but actually looks kind of cool — coloured text, boxes, borders, lists, menus, all drawn using characters. No windows. No buttons you click with a mouse. Just the terminal.

You've probably seen apps like this before. `htop`, `nano`, `vim` — those are TUI apps. That's the kind of thing you're building.

Some ideas of what you could make:
- A to-do list app
- A calculator
- A simple text editor
- A game like Snake or Tetris
- Space-related app
- Pretty much anything that works in a terminal

---

## The Rules (seriously, read these)

These are not just suggestions. If your app breaks any of them, it will get rejected from the library. No exceptions.

**1. TUI only. No GUI ever.**
Your app lives inside the terminal window. Don't open a separate window with `tkinter`. Don't use `pygame` or `turtle` or anything that draws outside the terminal. If a reviewer opens your app and a separate window pops up, it's an instant rejection.

**2. Python Standard Library only.**
This one is really important. You cannot use `pip install` or have a `requirements.txt`. No third-party packages at all. Everything you need has to come from Python's built-in standard library — the stuff that's already there when you install Python. The reason for this is that your app needs to work on any clean Python install, no setup required.

**3. Use the AstralixiOS API.**
Your app must import `astralixios_api` and use it for drawing things on screen. Don't write raw curses code. Don't call `os.system("clear")`. Don't print ANSI escape codes manually. The API handles all of that messy stuff for you, and it makes sure your app works properly on AstralixiOS.

**4. One file only.**
Your entire app has to be a single `.py` file. No importing from other files you wrote. No folders of modules. Just the one file. (This will probably change in a future version, but right now it's a hard requirement.)

**5. Always show how to exit.**
Q and Escape will quit any app automatically — the API handles that. But users don't know that unless you tell them. You MUST put the exit shortcut somewhere visible, like in the status bar at the bottom. If a reviewer can't figure out how to quit your app, it gets rejected.

---

## Setting Up

Grab `astralixios_api.py` from the AstralixiOS SDK and put it in the same folder as your app file. Your folder should look like this:

```
my_app/
├── astralixios_api.py   ← the API (you need this to develop locally)
└── my_app.py            ← your actual app
```

That's literally all the setup. When you submit your app, don't include `astralixios_api.py` in your repo — the OS already has it installed. You only need it locally for testing.

Run your app with:

```
python my_app.py
```

---

## Your First App — Hello World

Every AstralixiOS app follows the same three steps:

1. Create an `App` object (this is the thing you pass to everything)
2. Write a draw function (gets called 30 times per second to draw the screen)
3. Call `run()` to actually start the app

Here's the smallest valid app:

```python
from astralixios_api import App, draw_box, draw_text, draw_status_bar, run

def draw(app):
    draw_box(app, 0, 0, app.cols, app.rows, title="Hello World", double_border=True)
    draw_text(app, 4, 2, "Welcome to AstralixiOS!")
    draw_status_bar(app, " Q to quit")

app = App(title="Hello World")
run(app, draw)
```

Run it and you'll see a box with "Hello World" as the title, some text inside, and a status bar at the bottom. Press **Q** or **Escape** to quit.

Let's break down what each line is doing:

- `from astralixios_api import ...` — this imports the functions we need from the API
- `def draw(app):` — this is the draw function, which gets called every frame
- `draw_box(...)` — draws a rectangular border around the whole screen
- `draw_text(...)` — draws some text at column 4, row 2
- `draw_status_bar(...)` — draws a bar at the very bottom with the text " Q to quit"
- `app = App(title="Hello World")` — creates the App object
- `run(app, draw)` — starts the app, passing our draw function

---

## The App Object

The `App` object is the main thing that gets passed to every single API function. Think of it as the container for your whole app. Make one at the top and use it everywhere.

```python
app = App(title="My App", fps=30)
```

The `title` is just the name of your app. `fps` is how many times per second the draw function gets called — 30 is the default and is fine for most things.

Once your app is running, the `app` object has these useful bits of info you can read:

| Attribute | Type | What it tells you |
|-----------|------|-------------------|
| `app.cols` | int | How many columns wide the terminal is |
| `app.rows` | int | How many rows tall the terminal is |
| `app.scale_x` | float | How the width compares to the reference 720p screen |
| `app.scale_y` | float | How the height compares to the reference 720p screen |
| `app.title` | str | The app's current title (changeable with `set_title()`) |
| `app.running` | bool | True while the app is running, False once it's quitting |

The most useful ones are `app.cols` and `app.rows`. You'll use these ALL the time for positioning things on screen. They get measured once when you create the `App` object and stay fixed — AstralixiOS runs on a fixed 720p display and doesn't resize while the app is open.

---

## How the Screen Works

The terminal is basically a grid of character cells. Think of it like a giant spreadsheet where each cell is one character wide. The top-left corner is position `(col=0, row=0)`. Columns go right, rows go down.

```
(0,0) ────────────────────── (cols-1, 0)
  │                                  │
  │         your app lives here      │
  │                                  │
(0, rows-1) ─────────── (cols-1, rows-1)
```

So if you want to draw something in the top-left corner, you use col=0, row=0. If you want something near the bottom, you'd use a row close to `app.rows`. Makes sense, right?

### The 720p Reference Grid

AstralixiOS is designed for a 720p display, which works out to roughly **213 columns x 45 rows** at the character size AstralixiOS uses. That's your reference — when you design your app, imagine a 213x45 grid.

But don't hardcode those numbers! Always use `app.cols` and `app.rows` in your code so your layout adjusts correctly:

```python
# This is good — always works
panel_w = min(60, app.cols - 4)
panel_col = (app.cols - panel_w) // 2   # centres the panel horizontally

# This is bad — hardcoding will break things
panel_col = 76   # ← never do this
```

If you really want to design using the 720p reference coordinates and then convert, there's a `scale()` function for that. But honestly just using `app.cols` and `app.rows` directly is way simpler for most apps.

---

## The Draw Function — This is the Main One

Your draw function is called every single frame (30 times a second by default). It's responsible for drawing *everything* the user sees. The screen gets wiped clean before each call, so every frame you're starting from scratch.

```python
def draw(app):
    draw_box(app, 0, 0, app.cols, app.rows)
    draw_text(app, 2, 2, f"Score: {score}")
    draw_status_bar(app, " Q Quit  Arrow keys Move")
```

Some really important things to know about the draw function:

**Don't try to "remember" things between frames.** Because the screen is wiped each frame, you can't just draw something once and expect it to stay there. Every frame you draw everything from scratch. If you want something to appear on screen, draw it every frame.

**Keep it fast.** The draw function runs 30 times per second. If you put slow stuff in there — like reading a file, making a network request, or doing a big calculation — your app will feel laggy and gross. If you need to do heavy work in the background, use `threading.Thread` to do it in a separate thread, and then just read the result from a variable in your draw function.

```python
import threading

result = "loading..."   # shared variable

def fetch_data():
    global result
    # pretend this is a slow operation
    import time; time.sleep(2)
    result = "data loaded!"

threading.Thread(target=fetch_data, daemon=True).start()

def draw(app):
    draw_text(app, 2, 2, result)   # just reads the variable, super fast
```

---

## Handling Keyboard Input

To make your app actually do things when the user presses keys, you write an input function and pass it to `run()`. It gets called every time a key is pressed, and gives you a raw key code.

```python
def on_key(app, key):
    name = key_name(key)   # converts the raw number into something readable

    if name == "UP":
        move_up()
    elif name == "DOWN":
        move_down()
    elif name == "ENTER":
        select_item()
    elif is_printable(key):
        # the user typed a regular character like a letter or number
        text_buffer += chr(key)

run(app, draw, on_key)   # pass your input function as the third argument
```

`key_name()` converts the raw number into a readable string so you don't have to memorise key codes. Here are all the special key names:

| What the user pressed | What `key_name()` returns |
|-----------------------|---------------------------|
| Arrow keys | `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"` |
| Enter | `"ENTER"` |
| Backspace | `"BACKSPACE"` |
| Delete | `"DELETE"` |
| Tab | `"TAB"` |
| Escape | `"ESC"` |
| Home / End | `"HOME"`, `"END"` |
| Page Up / Down | `"PAGE_UP"`, `"PAGE_DOWN"` |
| F1 to F12 | `"F1"` through `"F12"` |
| Letters, numbers, symbols | `"a"`, `"B"`, `"1"`, `"!"`, etc. |

`is_printable(key)` returns `True` if the key is a regular typeable character (letters, numbers, punctuation, space). Use it to check if you should add the character to a text input.

### No mouse support

There's no mouse support at the moment. It'll be added in a future version, but even then it'll only be for scrolling — no clicking. So design your app to be 100% keyboard-driven. Arrow keys to move, Tab to switch between things, Enter to confirm, Escape to cancel. That's the pattern.

---

## Colours and Text Styles

### Colours

There are 8 colours you can use for text and backgrounds:

```python
COLOR_BLACK
COLOR_RED
COLOR_GREEN
COLOR_YELLOW
COLOR_BLUE
COLOR_MAGENTA
COLOR_CYAN
COLOR_WHITE
```

Use them in the `fg` (foreground/text colour) and `bg` (background colour) arguments of any draw function.

### Text Styles (Attributes)

You can also apply styles to text:

```python
ATTR_NORMAL     # plain, no styling
ATTR_BOLD       # bold or brighter
ATTR_DIM        # dimmer/faded
ATTR_UNDERLINE  # underlined
ATTR_BLINK      # blinking (doesn't work on every terminal)
ATTR_REVERSE    # swaps the text and background colours
ATTR_ITALIC     # italic (doesn't work on every terminal)
```

To combine multiple styles, use `|` between them:

```python
# Bold red text on a black background with an underline
draw_text(app, 2, 3, "WARNING!", fg=COLOR_RED, attr=ATTR_BOLD | ATTR_UNDERLINE)
```

---

## Drawing Things — Full Reference

### Text

```python
# Basic text at a position
draw_text(app, col, row, "Hello!")

# Text that auto-centres itself horizontally on the given row
draw_text_centered(app, row, "Centred heading")

# Multi-line text — supports \n and will word-wrap if it's too long
draw_multiline(app, col, row, "Line one\nLine two\nEtc")

# A label padded to a fixed width — great for aligning forms
draw_label(app, col, row, "Name:", width=12)
```

### Borders and Shapes

```python
# A simple single-line border box
draw_box(app, col, row, width, height)

# Same but with double-line borders (looks fancier)
draw_box(app, col, row, width, height, double_border=True)

# A box with a title in the top border
draw_box(app, col, row, width, height, title="My Panel")

# A horizontal line — great for dividers
draw_hline(app, col, row, length)

# A vertical line
draw_vline(app, col, row, length)

# Fill a rectangle with a colour (useful for coloured backgrounds)
fill_region(app, col, row, width, height, bg=COLOR_BLUE)

# Erase the whole screen (you usually won't need this — run() already does it each frame)
clear_screen(app)
```

A quick note on `draw_box`: the `width` and `height` include the border itself. So a box of width=10 has 8 usable columns inside (the border takes up the left and right columns). Same idea for height.

### Widgets

Widgets are the interactive bits — buttons, lists, inputs, etc. They handle the drawing, but your `on_key` function has to handle the logic (like what happens when the user presses Enter on a button).

**Button**
```python
# Draws [ Add ] or [ Add ] highlighted if focused=True
draw_button(app, col, row, "Add", focused=True)
draw_button(app, col, row, "Cancel", focused=False)
```
`draw_button()` returns the width of the button, which is handy for placing the next button right after it:
```python
x = 2
x += draw_button(app, x, row, "Yes", focused=True)
x += 1   # gap
x += draw_button(app, x, row, "No", focused=False)
```

**Checkbox**
```python
# Draws [X] Notifications  or  [ ] Notifications
draw_checkbox(app, col, row, "Notifications", checked=True, focused=False)
```

**Radio Button**
```python
# Draws (o) Option A  or  ( ) Option A
draw_radio(app, col, row, "Option A", selected=True, focused=False)
```

**Text Input**
```python
# Draws a text field. Your on_key function has to update the value variable.
draw_input(app, col, row, width=30, value=my_text, focused=True)

# With placeholder hint text (shows when empty and not focused)
draw_input(app, col, row, width=30, value=my_text, focused=False, placeholder="Search...")

# Password field (shows asterisks instead of text)
draw_input(app, col, row, width=20, value=my_password, password=True, focused=True)
```

Note: there's no visible cursor. When an input is focused it gets highlighted so the user knows they're typing in it. Your `on_key` function needs to handle `BACKSPACE` (to delete the last character) and `is_printable(key)` (to add characters).

**Scrollable List**
```python
draw_list(app, col, row, width=40, height=10,
          items=my_list, selected=cursor, scroll=offset)
```
`selected` is the index of the highlighted item. `scroll` is the index of the first visible item. Your `on_key` function moves `cursor` up and down, and the draw function clamps `scroll` so the selected item stays visible:
```python
if cursor < scroll:
    scroll = cursor
if cursor >= scroll + list_height:
    scroll = cursor - list_height + 1
```

**Progress Bar**
```python
# Shows a bar like [████████░░░░░░░░] 40%
draw_progress(app, col, row, width=40, value=40, max_value=100)
```

**Scrollbar**
```python
# Pair this with draw_list to show scroll position on the right side
draw_scrollbar(app, col, row, height=10,
               total=len(items), visible=10, scroll=offset)
```

**Menu Bar** (across the top row)
```python
draw_menu_bar(app, ["File", "Edit", "View", "Help"], selected=0)
```

**Status Bar** (across the bottom row — ALWAYS include your exit shortcut here)
```python
draw_status_bar(app, " Q Quit  Arrows Navigate  Enter Select")

# With extra text on the right side
draw_status_bar(app, " Q Quit", right_text="Ln 3, Col 12")
```

**Data Table**
```python
draw_table(app, col, row, width=60, height=8,
           headers=["Name", "Type", "Size"],
           rows=[
               ["readme.txt", "File", "2 KB"],
               ["photos/",    "Dir",  "—"],
           ])
```

**Modal Dialog** (a popup that appears over everything else)
```python
draw_modal(app, title="Confirm", body="Delete this file?",
           buttons=["Yes", "No"], focused_btn=1)
```
The modal appears as long as you keep calling `draw_modal()` in your draw function. To dismiss it, just stop calling it (usually controlled by a boolean variable like `showing_modal`).

**Animated Spinner** (for showing something is loading)
```python
import time
draw_spinner(app, col, row, frame=int(time.time() * 10), style="braille")
```
Styles you can pick from: `"braille"`, `"dots"`, `"line"`, `"box"`.

---

## A Full Example: Todo App

Here's a complete working app to show how everything fits together. It's a todo list where you can add and delete items.

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

    # Keep the selected item visible by clamping the scroll offset
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

## Other Useful Functions

### Notifications
```python
# Shows a little yellow banner at the bottom that disappears after 2 seconds
show_notification(app, "File saved!", duration=2.0)
```
Non-blocking — it doesn't pause your app. Just call it whenever you want and it handles itself.

### Updating the App Title
```python
set_title(app, "My App — Editing file.txt")
```
If your draw function shows `app.title` somewhere (like as a `draw_box` title), it'll update on the next frame automatically.

### Pausing
```python
# Pause for 0.5 seconds — use this instead of time.sleep() inside the app
sleep(0.5)
```

### Terminal Info (mostly for debugging)
```python
info = get_terminal_info()
# Returns a dict with cols, rows, colors, term, scale_x, scale_y
```

### Custom Colours
```python
# Set up a custom colour pair with ID 20 (red text on yellow background)
set_color_pair(20, COLOR_RED, COLOR_YELLOW)

# Then use it with draw_text using color_attr()
draw_text(app, 2, 4, "Alert!", attr=color_attr(20, ATTR_BOLD))
```
Useful if you want a specific colour combo that the default `fg`/`bg` system doesn't make easy.

---

## Testing Your App

Testing is simple — just run your `.py` file directly with Python. No special tools, no build step, nothing to install.

Make sure your folder looks like this:

```
my_app/
├── astralixios_api.py   ← grab this from the AstralixiOS SDK
└── my_app.py            ← your app
```

Because `astralixios_api` is imported by name, Python needs to find it in the same directory as your app. As long as it's sitting right next to your `.py` file, it'll work. Then just run:

```
python my_app.py
```

That's it. You'll see your app running exactly as it would on-device. Test all your key bindings, make sure the layout doesn't break at different terminal sizes (try resizing your terminal window before running — remember, `app.cols` and `app.rows` are measured at startup), and verify that the exit shortcut is clearly visible.

When you're done, do **NOT** include `astralixios_api.py` in your submission repo — the OS provides it automatically on-device.

### Getting Your App onto AstralixiOS

All `.axapp` files for AstralixiOS must be verified and compiled by Astroxia before they can be distributed or installed. There is no self-packaging or sideloading — this is intentional. Allowing anyone to freely compile and distribute `.axapp` bundles would create real security risks (malicious apps bypassing review) and decentralisation problems (unofficial app sources fragmenting the ecosystem). So the compilation and signing of `.axapp` bundles is handled entirely on Astroxia's end as part of the submission review process.

To get your app into the official library, open a public GitHub repository with your app. It needs:

```
my_app/
├── my_app.py     ← your app (single file only)
├── README.md     ← what the app does and how to use it
└── LICENSE       ← any open source licence you like
```

Then fork the AstralixiOS App Library repo and open a pull request adding your app to the `submissions/` folder, with an entry in `submissions/index.json` for your app's name, description, and category. A maintainer will review it manually for:

- TUI only — no GUI, no external windows
- Python Standard Library only — no third-party packages
- Uses `astralixios_api` correctly
- Has a visible exit option (in the status bar)
- README actually explains the app
- The app actually runs without errors

If something needs fixing you'll get a comment on the PR explaining what to change. Push the fix and the review continues. Apps that pass get compiled into a verified `.axapp`, merged into the library, and show up for users within 24 hours.

---

## Drawing Shapes

API v2 adds six shape-drawing functions. They all follow the same idea: you give a **centre point** `(cx, cy)`, a **size**, and choose whether you want the shape **filled or just an outline**. You can also control the foreground colour, background colour, and the characters used for the fill and border.

### How centre coords work

Every shape is centred on the `(cx, cy)` you provide, measured in actual terminal columns and rows. For example, to draw a shape in the middle of the screen:

```python
draw_circle(app, app.cols // 2, app.rows // 2, 6)
```

### Aspect-ratio correction

Terminal character cells are roughly twice as tall as they are wide. Every shape function automatically compensates for this by stretching horizontally, so a "circle" looks circular and a "square" looks square — not squashed. You don't need to do anything special; it's handled internally.

### Fill vs outline

Every shape accepts a `fill` parameter:

```python
# Solid filled circle
draw_circle(app, 40, 15, 5, fill=True, fg=COLOR_CYAN)

# Just the outline
draw_circle(app, 40, 15, 5, fill=False, fg=COLOR_CYAN)
```

You can also customise the characters used:

```python
# Fill with dots, border with a solid block
draw_square(app, 20, 10, 4, fill=True,
            fill_char="·", border_char="█", fg=COLOR_GREEN)
```

The defaults are `fill_char="█"` (solid block) and `border_char="▓"` (medium shade), which gives a nice visual distinction between the interior and the edge.

### The six shapes

**Square** — equal width and height, centred on the point. `size` is the half-height in rows; the width is automatically doubled to account for aspect ratio.

```python
draw_square(app, cx, cy, size, fill=True, fg=COLOR_WHITE, bg=COLOR_BLACK,
            fill_char="█", border_char="▓")
```

**Rectangle** — you control width and height independently. No automatic aspect-ratio correction here — you supply the exact column-span and row-span.

```python
draw_rectangle(app, cx, cy, half_w, half_h, fill=True, fg=COLOR_WHITE, bg=COLOR_BLACK,
               fill_char="█", border_char="▓")
```

**Circle** — `size` is the radius in rows. The horizontal radius is doubled internally for correct proportions.

```python
draw_circle(app, cx, cy, size, fill=True, fg=COLOR_WHITE, bg=COLOR_BLACK,
            fill_char="█", border_char="▓")
```

**Triangle** — upward-pointing isosceles triangle. `size` is the half-height; the base is at the bottom.

```python
draw_triangle(app, cx, cy, size, fill=True, fg=COLOR_WHITE, bg=COLOR_BLACK,
              fill_char="█", border_char="▓")
```

**Hexagon** — flat-top hexagon. `size` is the half-height in rows.

```python
draw_hexagon(app, cx, cy, size, fill=True, fg=COLOR_WHITE, bg=COLOR_BLACK,
             fill_char="█", border_char="▓")
```

**Octagon** — regular octagon with clipped corners. `size` is the half-height in rows.

```python
draw_octagon(app, cx, cy, size, fill=True, fg=COLOR_WHITE, bg=COLOR_BLACK,
             fill_char="█", border_char="▓")
```

### Full example — shape showcase

```python
from astralixios_api import (App, draw_box, draw_status_bar, run,
                              draw_square, draw_circle, draw_triangle,
                              draw_rectangle, draw_hexagon, draw_octagon,
                              COLOR_CYAN, COLOR_YELLOW, COLOR_RED,
                              COLOR_GREEN, COLOR_MAGENTA, COLOR_WHITE,
                              COLOR_BLACK)

def draw(app):
    draw_box(app, 0, 0, app.cols, app.rows, title="Shape Demo", double_border=True)

    row_y = app.rows // 2

    # Evenly spread six shapes across the screen
    positions = [
        app.cols * 1 // 7,
        app.cols * 2 // 7,
        app.cols * 3 // 7,
        app.cols * 4 // 7,
        app.cols * 5 // 7,
        app.cols * 6 // 7,
    ]

    draw_square   (app, positions[0], row_y, 4, fill=True,  fg=COLOR_CYAN)
    draw_rectangle(app, positions[1], row_y, 12, 3, fill=False, fg=COLOR_YELLOW)
    draw_circle   (app, positions[2], row_y, 4, fill=True,  fg=COLOR_RED)
    draw_triangle (app, positions[3], row_y, 4, fill=False, fg=COLOR_GREEN)
    draw_hexagon  (app, positions[4], row_y, 4, fill=True,  fg=COLOR_MAGENTA)
    draw_octagon  (app, positions[5], row_y, 4, fill=False, fg=COLOR_WHITE)

    draw_status_bar(app, " Q to quit")

app = App(title="Shape Demo")
run(app, draw)
```

---



**Keyboard navigation is everything.** Your app must be completely usable without a mouse (there isn't one anyway). Arrow keys to move, Tab to cycle between focusable things, Enter to confirm, Escape to cancel or go back. If something is only reachable with a click, it's not accessible at all.

**Always tell users the controls.** Use `draw_status_bar()` to show the key bindings for whatever screen the user is on right now. ALWAYS include the quit shortcut. This is a hard requirement for review AND it's just good manners.

**Use `app.cols` and `app.rows` for EVERYTHING.** Never type a specific number for a position or size. Your layout should be calculated from `app.cols` and `app.rows` every frame. That way it works correctly at whatever terminal size is running.

**Keep the draw function fast.** It runs 30 times a second. Anything slow in there makes the whole app feel awful. Move slow work into a background thread.

**Test at 80x24.** That's a really common terminal size. Make sure your layout doesn't break or overflow at that size — some users might run your app on a regular terminal before installing it.

**Use global variables for state.** Since your draw function and input function are both separate functions, you'll use `global` variables to share state between them. This is fine for AstralixiOS apps — see the todo app example above.

---

## Quick Reference — All Functions

| Function | What it does |
|----------|--------------|
| `App(title, fps)` | Create the app object |
| `run(app, draw_fn, input_fn)` | Start the main loop |
| `quit(app)` | Exit the loop from code |
| `get_size(app)` | Returns `(cols, rows)` |
| `scale(app, ref_col, ref_row)` | Convert 720p reference coords to actual coords |
| `draw_text(...)` | Draw a string at a position |
| `draw_text_centered(...)` | Draw a string centred on a row |
| `draw_multiline(...)` | Draw text with newlines and word-wrap |
| `draw_label(...)` | Fixed-width padded label |
| `draw_box(...)` | Rectangular border (single or double, optional title) |
| `draw_hline(...)` | Horizontal line |
| `draw_vline(...)` | Vertical line |
| `fill_region(...)` | Filled coloured rectangle |
| `clear_screen(app)` | Erase the screen |
| `draw_button(...)` | Focusable button, returns its width |
| `draw_checkbox(...)` | Tickbox — `[X] Label` |
| `draw_radio(...)` | Radio button — `(o) Label` |
| `draw_input(...)` | Text input field |
| `draw_list(...)` | Scrollable list |
| `draw_progress(...)` | Progress bar |
| `draw_scrollbar(...)` | Vertical scroll indicator |
| `draw_menu_bar(...)` | Menu bar across the top row |
| `draw_status_bar(...)` | Status bar across the bottom row |
| `draw_table(...)` | Data table with column headers |
| `draw_modal(...)` | Centred popup dialog |
| `draw_spinner(...)` | Animated single-character spinner |
| `draw_square(...)` | Filled or outlined square centred on a point |
| `draw_rectangle(...)` | Filled or outlined rectangle centred on a point |
| `draw_circle(...)` | Filled or outlined circle centred on a point |
| `draw_triangle(...)` | Filled or outlined upward triangle centred on a point |
| `draw_hexagon(...)` | Filled or outlined flat-top hexagon centred on a point |
| `draw_octagon(...)` | Filled or outlined octagon centred on a point |
| `key_name(key)` | Convert raw key code to readable name |
| `is_printable(key)` | True if the key is a typeable character |
| `set_color_pair(id, fg, bg)` | Define a custom colour pair |
| `color_attr(id, attr)` | Build an attribute int from a colour pair |
| `get_terminal_info()` | Dict of terminal info (cols, rows, etc) |
| `show_notification(app, msg, duration)` | Timed pop-up banner at the bottom |
| `set_title(app, title)` | Update app.title |
| `sleep(seconds)` | Non-blocking pause |

---

*AstralixiOS Developer Documentation v16-05-2026  —  API v2*
