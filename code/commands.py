import os
import signal
import subprocess
import shutil
import math
import psutil
import platform

# ── State ──────────────────────────────────────────────────────────────────────

command_history_log = []   # Tracks every successfully run command
_shortcuts         = {}    # Stores aka aliases: { shortcut: full_command }

# Credentials are stored in RAM only — they are NOT persisted to disk.
# Populated at startup by main.py reading credentials.txt (legacy bootstrap),
# or left empty if no credentials file exists.
_credentials = {
    "username": "",
    "password": "",
}

_RAM_WARNING = (
    "[!] This change is saved in RAM only and will be lost on shutdown or reboot.\n"
    "[i] Permanent credential storage is coming in a future version of Astralixi OS."
)

def _print_ram_warning():
    """Print the RAM-storage warning whenever a credential is changed."""
    print(_RAM_WARNING)

# ── Command dispatcher ─────────────────────────────────────────────────────────

def execute_command(command):
    """Parse and dispatch a command string to its handler function."""
    parts = command.split()
    if not parts:
        return
    cmd, *args = parts

    COMMANDS = {
        # Files
        'lf':     list_files,
        'mkf':    create_file,
        'rm':     remove_file,
        'cp':     copy_file,
        'mv':     move_file,
        'rn':     rename_file,
        'prf':    print_file,
        'search': search_for_file,
        # Directories
        'ld':     list_directories,
        'cd':     change_directory,
        'pcwd':   print_current_directory,
        'mkdir':  make_folder,
        'rmdir':  remove_folder,
        'cpdir':  copy_folder,
        'mvdir':  move_folder,
        'rndir':  rename_folder,
        # System
        'ps':       processes_running,
        'kill':     kill_process,
        'df':       disk_free,
        'mem':      memory_used,
        'clear':    clear_terminal,
        'history':  command_history,
        'whoami':   who_am_i,
        'username': change_username,
        'password': change_password,
        # Space
        'orbit':         orbital_speed,
        'rocket':        rocket_equation,
        'planets':       planet_reference,
        'launchsites':   launch_sites,
        'phases':        moon_phases,
        'timeinspace':   time_in_space,
        'constellation': constellation_reference,
        # Misc
        'aka':    shortcut_to_long_command,
        'help':   help_manual,
        'hello':  hello,
        'pyrun':  pyrun,
    }

    if cmd in COMMANDS:
        try:
            COMMANDS[cmd](*args)
            command_history_log.append(command)
        except TypeError as e:
            print(f"Error: Wrong number of arguments for '{cmd}'. {e}")
        except Exception as e:
            print(f"Error running '{cmd}': {e}")
    else:
        print(f"Command '{cmd}' not found. Type 'help' for a list of commands.")


# ══════════════════════════════════════════════════════════════════════════════
#  FILE COMMANDS
# ══════════════════════════════════════════════════════════════════════════════

def list_files():
    """List only files (not directories) in the current directory."""
    cwd   = os.getcwd()
    files = [f for f in os.listdir(cwd) if os.path.isfile(os.path.join(cwd, f))]
    if files:
        for f in files:
            print(f)
    else:
        print("No files found.")


def create_file(name):
    """Create a new empty file."""
    if os.path.exists(name):
        print(f"Error: '{name}' already exists.")
        return
    with open(name, "w"):
        pass
    print(f"File '{name}' created.")


def remove_file(path):
    """Delete a file (not a directory — use rmdir for that)."""
    if os.path.isdir(path):
        print(f"Error: '{path}' is a directory. Use 'rmdir' instead.")
        return
    try:
        os.remove(path)
        print(f"File '{path}' removed.")
    except FileNotFoundError:
        print(f"Error: File '{path}' not found.")


def copy_file(source, destination):
    """Copy a file to a new location."""
    if os.path.isdir(source):
        print(f"Error: '{source}' is a directory. Use 'cpdir' instead.")
        return
    try:
        shutil.copy(source, destination)
        print(f"'{source}' copied to '{destination}'.")
    except FileNotFoundError:
        print(f"Error: File '{source}' not found.")


def move_file(source, destination):
    """Move a file to a new location."""
    if os.path.isdir(source):
        print(f"Error: '{source}' is a directory. Use 'mvdir' instead.")
        return
    try:
        shutil.move(source, destination)
        print(f"'{source}' moved to '{destination}'.")
    except FileNotFoundError:
        print(f"Error: File '{source}' not found.")


def rename_file(old_name, new_name):
    """Rename a file."""
    if os.path.isdir(old_name):
        print(f"Error: '{old_name}' is a directory. Use 'rndir' instead.")
        return
    try:
        os.rename(old_name, new_name)
        print(f"File renamed to '{new_name}'.")
    except FileNotFoundError:
        print(f"Error: File '{old_name}' not found.")
    except FileExistsError:
        print(f"Error: '{new_name}' already exists.")


def print_file(name):
    """Print the full contents of a file to the terminal."""
    if os.path.isdir(name):
        print(f"Error: '{name}' is a directory, not a file.")
        return
    try:
        with open(name, "r") as f:
            print(f.read())
    except FileNotFoundError:
        print(f"Error: File '{name}' not found.")

def search_for_file(search_term):
    """Search for files (not directories) whose names contain the given term."""
    cwd     = os.getcwd()
    results = [
        f for f in os.listdir(cwd)
        if search_term.lower() in f.lower() and os.path.isfile(os.path.join(cwd, f))
    ]
    if results:
        for r in results:
            print(r)
    else:
        print(f"No files matching '{search_term}' found.")


# ══════════════════════════════════════════════════════════════════════════════
#  DIRECTORY COMMANDS
# ══════════════════════════════════════════════════════════════════════════════

def list_directories():
    """List only directories (not files) in the current directory."""
    cwd  = os.getcwd()
    dirs = [d for d in os.listdir(cwd) if os.path.isdir(os.path.join(cwd, d))]
    if dirs:
        for d in dirs:
            print(d)
    else:
        print("No directories found.")


def change_directory(new_directory):
    """Change the current working directory."""
    try:
        os.chdir(new_directory)
    except FileNotFoundError:
        print(f"Error: Directory '{new_directory}' not found.")
    except NotADirectoryError:
        print(f"Error: '{new_directory}' is not a directory.")


def print_current_directory():
    """Print the current working directory path."""
    print(os.getcwd())


def make_folder(path):
    """Create a new directory."""
    try:
        os.mkdir(path)
        print(f"Directory '{path}' created.")
    except FileExistsError:
        print(f"Error: Directory '{path}' already exists.")
    except FileNotFoundError:
        print(f"Error: Parent path not found.")


def remove_folder(path):
    """Remove a directory and all its contents."""
    if os.path.isfile(path):
        print(f"Error: '{path}' is a file. Use 'rm' instead.")
        return
    try:
        shutil.rmtree(path)
        print(f"Directory '{path}' removed.")
    except FileNotFoundError:
        print(f"Error: Directory '{path}' not found.")


def copy_folder(source, destination):
    """Recursively copy a directory and all its contents."""
    if os.path.isfile(source):
        print(f"Error: '{source}' is a file. Use 'cp' instead.")
        return
    try:
        shutil.copytree(source, destination)
        print(f"'{source}' copied to '{destination}'.")
    except FileNotFoundError:
        print(f"Error: Directory '{source}' not found.")
    except FileExistsError:
        print(f"Error: '{destination}' already exists.")


def move_folder(source, destination):
    """Move a directory to a new location."""
    if os.path.isfile(source):
        print(f"Error: '{source}' is a file. Use 'mv' instead.")
        return
    try:
        shutil.move(source, destination)
        print(f"'{source}' moved to '{destination}'.")
    except FileNotFoundError:
        print(f"Error: Directory '{source}' not found.")


def rename_folder(old_name, new_name):
    """Rename a directory."""
    if os.path.isfile(old_name):
        print(f"Error: '{old_name}' is a file. Use 'rn' instead.")
        return
    try:
        os.rename(old_name, new_name)
        print(f"Directory renamed to '{new_name}'.")
    except FileNotFoundError:
        print(f"Error: Directory '{old_name}' not found.")
    except FileExistsError:
        print(f"Error: '{new_name}' already exists.")


# ══════════════════════════════════════════════════════════════════════════════
#  SYSTEM COMMANDS
# ══════════════════════════════════════════════════════════════════════════════

def processes_running():
    """List all running processes with their PID and name."""
    for proc in psutil.process_iter(['pid', 'name']):
        try:
            print(f"PID: {proc.info['pid']:<8} Name: {proc.info['name']}")
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass


def kill_process(selected_pid):
    """Kill a process by its PID."""
    try:
        pid = int(selected_pid)
        if pid == 1:
            print("Error: Cannot kill PID 1 — that is Astralixi OS itself. Doing so would kernel panic.")
            return
        os.kill(pid, signal.SIGKILL)
        print(f"Process {pid} terminated.")
    except ValueError:
        print("Error: PID must be a number.")
    except ProcessLookupError:
        print(f"Error: No process with PID {selected_pid} found.")
    except PermissionError:
        print(f"Error: Insufficient permissions to kill PID {selected_pid}.")


def clear_terminal():
    """Clear the terminal screen using a direct ANSI escape sequence."""
    # os.system('clear') requires a shell binary — use raw ANSI instead,
    # which works reliably when running as PID 1 with no shell present.
    print("\033[2J\033[H", end="", flush=True)


def command_history():
    """Print the last 25 commands that were run."""
    if not command_history_log:
        print("No command history yet.")
        return
    recent = command_history_log[-25:]
    print("Command History:")
    for i, cmd in enumerate(recent, 1):
        print(f"  {i:2}. {cmd}")


def disk_free():
    """Show total, used, and free disk space for the current drive."""
    usage = shutil.disk_usage(os.getcwd())
    gb    = 1024 ** 3
    print(
        f"Total: {usage.total / gb:.2f} GB  |  "
        f"Used:  {usage.used  / gb:.2f} GB  |  "
        f"Free:  {usage.free  / gb:.2f} GB"
    )


def memory_used():
    """Show current RAM usage (total, used, available, and percent)."""
    mem = psutil.virtual_memory()
    gb  = 1024 ** 3
    print(
        f"Total: {mem.total     / gb:.2f} GB  |  "
        f"Used:  {mem.used      / gb:.2f} GB  |  "
        f"Available: {mem.available / gb:.2f} GB  |  "
        f"{mem.percent}% in use"
    )


# ══════════════════════════════════════════════════════════════════════════════
#  ACCOUNT COMMANDS
# ══════════════════════════════════════════════════════════════════════════════

def who_am_i():
    """Print the current username stored in RAM."""
    username = _credentials["username"]
    if username:
        print(f"Username: {username}")
    else:
        print("No username set.")


def change_username():
    """Update the stored username after verifying the current one."""
    current = _credentials["username"]
    if not current:
        print("No username is currently set.")
        return
    old_check = input("Current Username: ").strip()
    if old_check != current:
        print("Incorrect username.")
        return
    new_username = input("New Username: ").strip()
    if not new_username:
        print("Username cannot be empty.")
        return
    _credentials["username"] = new_username
    print(f"Username updated to '{new_username}'.")
    _print_ram_warning()


def change_password():
    """Update the stored password after verifying the current one."""
    current = _credentials["password"]
    if not current:
        print("No password is currently set.")
        return
    old_check = input("Current Password: ").strip()
    if old_check != current:
        print("Incorrect password.")
        return
    new_password = input("New Password: ").strip()
    if not new_password:
        print("Password cannot be empty.")
        return
    while True:
        confirm = input("Confirm New Password: ").strip()
        if confirm == new_password:
            break
        print("Passwords do not match. Try again.")
    _credentials["password"] = new_password
    print("Password updated.")
    _print_ram_warning()


# ══════════════════════════════════════════════════════════════════════════════
#  SPACE COMMANDS
# ══════════════════════════════════════════════════════════════════════════════

def orbital_speed(planet):
    """Show orbital speed, period, and distance for Earth (around Sun) or Moon (around Earth)."""
    # Define constants
    earth_mass = 5.972e24   # kg (unused but kept for context)
    moon_mass  = 7.342e22   # kg

    planet = planet.lower()

    # --- Orbital speed (km/s) ---
    if planet == 'earth':
        speed = 29.78          # km/s around the Sun
    elif planet == 'moon':
        speed = 1.022          # km/s around the Earth
    else:
        print("Data unavailable. Valid options: earth, moon")
        return

    # --- Orbital period ---
    if planet == 'earth':
        total_days   = 365.25          # days — one trip around the Sun
        total_months = total_days / 30.4375
        total_years  = total_days / 365.25
    elif planet == 'moon':
        total_days   = 27.32           # days — one trip around the Earth (sidereal)
        total_months = total_days / 30.4375
        total_years  = total_days / 365.25

    # --- Average orbital distance ---
    if planet == 'earth':
        average_distance = 149_600_000  # km from the Sun
        reference        = "the Sun"
    elif planet == 'moon':
        average_distance = 384_400      # km from the Earth
        reference        = "the Earth"

    print(f"Average orbital speed:  {speed:.3f} km/s")
    print(f"Orbital period:         {total_days:.2f} days  |  "
          f"{total_months:.2f} months  |  {total_years:.4f} years")
    print(f"Average distance from {reference}: {average_distance:,} km")


def rocket_equation(isp, m0, mf):
    """
    Calculate delta-v using the Tsiolkovsky rocket equation.
    Usage:   rocket <isp> <initial_mass_kg> <final_mass_kg>
    Example: rocket 311 500000 200000
    Isp is specific impulse in seconds (e.g. Merlin 1D vacuum: 311 s).
    """
    try:
        isp = float(isp)
        m0  = float(m0)
        mf  = float(mf)
    except ValueError:
        print("Error: All arguments must be numbers.")
        return

    if mf <= 0 or m0 <= 0:
        print("Error: Masses must be greater than zero.")
        return
    if mf >= m0:
        print("Error: Final mass must be less than initial mass (fuel must be consumed).")
        return

    g0    = 9.80665               # standard gravity m/s²
    dv    = isp * g0 * math.log(m0 / mf)
    dv_km = dv / 1000

    print(f"Isp:              {isp:.1f} s")
    print(f"Initial mass:     {m0:,.0f} kg")
    print(f"Final mass:       {mf:,.0f} kg")
    print(f"Mass ratio:       {m0 / mf:.4f}")
    print(f"Delta-v:          {dv:,.2f} m/s  |  {dv_km:.4f} km/s")


def planet_reference():
    """Print a quick-reference table of key data for all eight solar system planets."""
    planets = [
        # name, dist(AU), period(yr), diameter(km), moons
        ("Mercury",  0.387,   0.241,    4_879,    0),
        ("Venus",    0.723,   0.615,   12_104,    0),
        ("Earth",    1.000,   1.000,   12_742,    1),
        ("Mars",     1.524,   1.881,    6_779,    2),
        ("Jupiter",  5.203,  11.860,  139_820,   95),
        ("Saturn",   9.537,  29.457,  116_460,  146),
        ("Uranus",  19.191,  84.011,   50_724,   28),
        ("Neptune", 30.069, 164.795,   49_244,   16),
    ]
    print(f"{'Planet':<10}  {'Dist (AU)':>10}  {'Period (yr)':>12}  {'Diameter (km)':>14}  {'Moons':>6}")
    print("─" * 62)
    for name, dist, period, diameter, moons in planets:
        print(f"{name:<10}  {dist:>10.3f}  {period:>12.3f}  {diameter:>14,}  {moons:>6}")


def launch_sites():
    """Print a reference list of major rocket launch sites around the world."""
    sites = [
        ("Kennedy Space Center LC-39A",  "USA",        28.6,   -80.6,  "Falcon 9/Heavy, Crew Dragon"),
        ("Vandenberg SFB SLC-4E",        "USA",        34.6,  -120.6,  "Falcon 9, polar orbits"),
        ("Baikonur Cosmodrome",          "Kazakhstan", 45.9,   63.3,   "Soyuz, Proton"),
        ("Guiana Space Centre ELA-4",    "France/ESA",  5.2,  -52.8,   "Ariane 6"),
        ("Satish Dhawan Space Centre",   "India",      13.7,   80.2,   "PSLV, GSLV"),
        ("Tanegashima Space Centre",     "Japan",      30.4,  130.9,   "H-IIA, H3"),
        ("Wenchang Space Launch Site",   "China",      19.6,  110.9,   "Long March 5/7"),
        ("Starbase (Boca Chica)",        "USA",        25.9,  -97.2,   "Starship"),
    ]
    print(f"{'Site':<36}  {'Country':<14}  {'Lat':>6}  {'Lon':>7}  {'Vehicles'}")
    print("─" * 90)
    for name, country, lat, lon, vehicles in sites:
        print(f"{name:<36}  {country:<14}  {lat:>6.1f}  {lon:>7.1f}  {vehicles}")


def moon_phases():
    """
    Show the approximate current moon phase based on a known reference new moon.
    Uses the J2000 epoch reference and a synodic period of 29.53059 days.
    """
    import time as _time

    # Reference new moon: 2000-01-06 18:14 UTC expressed as a Unix timestamp
    reference_new_moon = 947182440.0   # seconds since Unix epoch
    synodic_period     = 29.53059      # days

    now_ts     = _time.time()
    elapsed    = (now_ts - reference_new_moon) / 86400   # convert to days
    phase_days = elapsed % synodic_period                 # days into current cycle
    fraction   = phase_days / synodic_period              # 0.0 – 1.0

    # Map fraction to a named phase
    if fraction < 0.0625 or fraction >= 0.9375:
        phase_name = "New Moon"
    elif fraction < 0.1875:
        phase_name = "Waxing Crescent"
    elif fraction < 0.3125:
        phase_name = "First Quarter"
    elif fraction < 0.4375:
        phase_name = "Waxing Gibbous"
    elif fraction < 0.5625:
        phase_name = "Full Moon"
    elif fraction < 0.6875:
        phase_name = "Waning Gibbous"
    elif fraction < 0.8125:
        phase_name = "Last Quarter"
    else:
        phase_name = "Waning Crescent"

    illumination = round((1 - math.cos(2 * math.pi * fraction)) / 2 * 100, 1)

    print(f"Days into cycle:  {phase_days:.2f} / {synodic_period} days")
    print(f"Phase:            {phase_name}")
    print(f"Illumination:     ~{illumination}%")


def time_in_space(start_date, end_date):
    """
    Calculate the duration between two dates (e.g. a mission window).
    Usage:   timeinspace <YYYY-MM-DD> <YYYY-MM-DD>
    Example: timeinspace 2024-03-04 2024-09-07
    """
    import datetime

    try:
        d1 = datetime.date.fromisoformat(start_date)
        d2 = datetime.date.fromisoformat(end_date)
    except ValueError:
        print("Error: Dates must be in YYYY-MM-DD format.")
        return

    if d2 < d1:
        d1, d2 = d2, d1   # swap so delta is always positive

    delta   = d2 - d1
    days    = delta.days
    weeks   = days // 7
    months  = days / 30.4375
    years   = days / 365.25

    print(f"From:     {d1}")
    print(f"To:       {d2}")
    print(f"Duration: {days} days  |  {weeks} weeks  |  {months:.2f} months  |  {years:.4f} years")


def constellation_reference():
    """Print a reference list of notable constellations and their brightest star."""
    constellations = [
        ("Orion",        "Rigel",          "Hunter — one of the most recognisable in the night sky"),
        ("Ursa Major",   "Alioth",         "Great Bear — contains the Big Dipper asterism"),
        ("Ursa Minor",   "Polaris",        "Little Bear — Polaris marks true north"),
        ("Cassiopeia",   "Schedar",        "Queen — distinctive W-shape, circumpolar from mid-latitudes"),
        ("Scorpius",     "Antares",        "Scorpion — prominent in southern summer skies"),
        ("Leo",          "Regulus",        "Lion — spring constellation of the zodiac"),
        ("Cygnus",       "Deneb",          "Swan — contains the Northern Cross asterism"),
        ("Crux",         "Acrux",          "Southern Cross — used for navigation in the southern hemisphere"),
        ("Centaurus",    "Alpha Centauri", "Centaur — contains the nearest star system to the Sun"),
        ("Lyra",         "Vega",           "Lyre — Vega is one of the brightest stars in the sky"),
        ("Aquila",       "Altair",         "Eagle — part of the Summer Triangle"),
        ("Gemini",       "Pollux",         "Twins — zodiac constellation, visible in northern winter"),
    ]
    print(f"{'Constellation':<14}  {'Brightest Star':<16}  Notes")
    print("─" * 75)
    for name, star, notes in constellations:
        print(f"{name:<14}  {star:<16}  {notes}")


# ══════════════════════════════════════════════════════════════════════════════
#  MISC COMMANDS
# ══════════════════════════════════════════════════════════════════════════════

def hello():
    print("Hello!")


def pyrun(script_path):
    """
    Run a Python script file.
    [!] Not yet implemented — coming in a future version of Astralixi OS.
    """
    print("[!] 'pyrun' is not yet implemented.")
    print("[i] Python script execution is planned for a future version of Astralixi OS.")

def shortcut_to_long_command(shortcut, *cmd_parts):
    """
    Create a shortcut alias for a longer command.
    Usage:   aka <shortcut> <command>
    Example: aka ll lf
    """
    if not cmd_parts:
        print("Usage: aka <shortcut> <command>")
        return
    full_command        = " ".join(cmd_parts)
    _shortcuts[shortcut] = full_command
    print(f"Shortcut '{shortcut}' -> '{full_command}' saved.")


def help_manual():
    """Print all available commands and their usage."""
    print("""
Available Commands
══════════════════════════════════════════════════════════
  File Operations:
    lf                         List files in current directory
    mkf  <name>                Create a new file
    rm   <path>                Delete a file
    cp   <src> <dst>           Copy a file
    mv   <src> <dst>           Move a file
    rn   <old> <new>           Rename a file
    prf  <name>                Print file contents
    search <term>              Search for files by name

  Directory Operations:
    ld                         List directories in current directory
    cd   <path>                Change directory
    pcwd                       Print current directory path
    mkdir <path>               Create a directory
    rmdir <path>               Delete an empty directory
    cpdir <src> <dst>          Copy a directory (recursive)
    mvdir <src> <dst>          Move a directory
    rndir <old> <new>          Rename a directory

  System:
    ps                         List running processes
    kill <pid>                 Kill a process by PID
    df                         Show disk usage
    mem                        Show RAM usage
    clear                      Clear the terminal
    history                    Show last 25 commands
    whoami                     Print current username
    username                   Change stored username
    password                   Change stored password

  Space:
    orbit <planet/moon>        Orbital speed, period, distance (earth/moon)
    rocket <isp> <m0> <mf>     Delta-v via Tsiolkovsky rocket equation
    planets                    Quick-reference table for all 8 planets
    launchsites                Major rocket launch sites around the world
    phases                     Current moon phase and illumination
    timeinspace <d1> <d2>      Mission duration between two dates (YYYY-MM-DD)
    constellation              Notable constellations and their brightest stars

  Misc:
    hello                      Says "hello" to you!
    aka  <alias> <cmd>         Create a command shortcut
    help                       Show this help message
══════════════════════════════════════════════════════════
""")
