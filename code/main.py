import commands
import time
import readline
import os
import signal

# ── Kernel filesystem bootstrap ────────────────────────────────────────────────
# Must run before anything else. /proc is required by psutil (ps, kill, mem).
# /dev is required for terminal I/O (readline, input). /tmp is required by
# some C runtime internals. Failures are non-fatal — we continue regardless.
def _bootstrap():
    for cmd in (
        "mount -t proc     proc     /proc",
        "mount -t sysfs    sys      /sys",
        "mount -t devtmpfs dev      /dev",
        "mount -t tmpfs    tmpfs    /tmp",
    ):
        os.system(cmd)

_bootstrap()

try:
    os.nice(-20)   # Sets highest process priority — works when run as root
except PermissionError:
    pass           # Silently continues if not root

COMMAND_LIST = [
    'lf', 'mkf', 'rm', 'cp', 'mv', 'rn', 'prf', 'search',
    'ld', 'cd', 'pcwd', 'mkdir', 'rmdir', 'cpdir', 'mvdir', 'rndir',
    'ps', 'kill', 'df', 'mem', 'clear', 'history', 'whoami', 'username',
    'password', 'pyrun',
    'orbit', 'rocket', 'planets', 'launchsites', 'phases', 'timeinspace',
    'constellation',
    'aka', 'help',
]

# Completes commands based on input text in the terminal
def _completer(text, state):
    # Checks input text to see the closest command to it (if none, returns None)
    matches = [c for c in COMMAND_LIST if c.startswith(text)]
    return matches[state] if state < len(matches) else None

# binds tab key to use the function, and set's the function to be used for completion 
readline.set_completer(_completer)
readline.parse_and_bind('tab: complete')

# credentials.txt for first full release of Astralixi OS with saved credentials on disk.
CREDENTIALS_FILE = "credentials.txt"

# Checks if credentials.txt has the user's credentials
def credentials_are_set():
    # Return True if credentials.txt has a non-empty username or password.
    try:
        with open(CREDENTIALS_FILE, "r") as f:
            for line in f:
                if line.startswith("Username:"):
                    value = line.split(":", 1)[1].strip()
                    if value:
                        return True
                if line.startswith("Password:"):
                    value = line.split(":", 1)[1].strip()
                    if value:
                        return True
    except FileNotFoundError:
        pass
    return False

def login():
    # Prompt the user to log in. Allows 3 attempts before exiting.
    print("-- Login --")
    attempts = 3
    while attempts > 0:
        # looks for credentials.txt, if not found, system gets halted
        try:
            with open(CREDENTIALS_FILE, "r") as f:
                lines = f.readlines()
        except FileNotFoundError:
            print(f"Error: '{CREDENTIALS_FILE}' not found.")
            # PID 1 cannot call exit() cleanly — halt the system instead
            os.system("halt -f")
            while True:
                time.sleep(3600)
        # To store username and password in variables
        stored_username = ""
        stored_password = ""
        # To seperate "Username:" and e.g "Astrox" in the file
        for line in lines:
            if line.startswith("Username:"):
                stored_username = line.split(":", 1)[1].strip()
            elif line.startswith("Password:"):
                stored_password = line.split(":", 1)[1].strip()

        username_input = input("Username: ").strip()
        password_input = input("Password: ").strip()
        
        # Checks if correct credentials inputed by the user
        if username_input == stored_username and password_input == stored_password:
            print(f"Welcome, {stored_username}!\n")
            # Seed in-RAM credentials so whoami/username/password commands work
            commands._credentials["username"] = stored_username
            commands._credentials["password"] = stored_password
            return
        # If inputed credentials incorrect, user's remaining attempts counter goes down
        else:
            attempts -= 1
            if attempts > 0:
                print(f"Incorrect credentials. {attempts} attempt(s) remaining.\n")
            else:
                print("Too many failed attempts. Halting.")
                os.system("halt -f")
                while True:
                    time.sleep(3600)

if credentials_are_set():
    login()

while True:
    try:
        # Command prompt
        command = input("> ")
        # Detemines the length of commands history logs
        prev_len = len(commands.command_history_log)
        # instruction to execute command given by user
        commands.execute_command(command)
        # Checks if number of cmds used increased since last check,
        # if so, adds the recently executed command to the list
        if len(commands.command_history_log) > prev_len:
            readline.add_history(command)
            # Checks if history log's length is longer than 25, the 1st item
            # of the list is removed, so an item can be added at the end
            if readline.get_current_history_length() > 25:
                readline.remove_history_item(0)
        # Delay to avoid CPU overload
        time.sleep(0.1)
    # ctrl+c and other interrupts don't work, as Astralixi OS is the operating system
    except KeyboardInterrupt:
        print("\nKeyboard interrupt ignored — Astralixi OS is PID 1 and cannot exit.")
    # Lets you extract any other error that comes up, and store the error details
    except Exception as e:
        # PID 1 must never crash out. Log the error and continue.
        print(f"\n[!] Unhandled error in main loop: {e}")
        print("[i] Astralixi OS is recovering — press Enter to continue.")

# ── PID 1 safety net ──────────────────────────────────────────────────────────
# Execution should never reach here. If it does, hang indefinitely rather than
# letting the kernel panic.
print("[!] Astralixi OS main loop exited unexpectedly. Halting.")
os.system("halt -f")
while True:
    time.sleep(3600)
