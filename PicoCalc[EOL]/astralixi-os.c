// cmake lists is at "C:\Program Files\CMake\bin\cmake.exe"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "drivers/picocalc.h"
#include "drivers/display.h"
#include "drivers/lcd.h"
#include "drivers/keyboard.h"
#include "drivers/sdcard.h"
#include "drivers/audio.h"
#include "drivers/fat32.h"
#include "drivers/southbridge.h"
#include "hardware/watchdog.h"
#include "psram_spi.h"

#define MAX_COMMAND_LENGTH 129 // 128 characters + null terminator
#define MAX_USERNAME_LENGTH 15 // 14 + null
#define MAX_PASSWORD_LENGTH 15

// RP2040-PSRAM required defines
#define PSRAM_PIN_CS   20
#define PSRAM_PIN_SCK  21
#define PSRAM_PIN_MOSI 2
#define PSRAM_PIN_MISO 3

// Declare PSRAM handle globally
psram_spi_inst_t psram_spi;

void read_line(char *buffer, int maxLength);
void read_password(char *buffer, int maxLength);
void safe_gets(char *buffer, int maxLength);

char currentUsername[MAX_USERNAME_LENGTH]; 
char currentPassword[MAX_PASSWORD_LENGTH];
char enteredUsername[MAX_USERNAME_LENGTH];
char enteredPassword[MAX_PASSWORD_LENGTH];
char currentWorkingDirectory[FAT32_MAX_PATH_LEN] = "/";
int failedLoginAttempts = 0;

int mini_seconds = 0;
int seconds = 0;
int minutes = 00;
int hours = 00;

volatile bool isTypingAllowed = false;

fat32_file_t dir;
fat32_entry_t entry;

#define MAX_HISTORY 25

char history[MAX_HISTORY][MAX_COMMAND_LENGTH];
int history_next_idx = 0;
int history_cycle_idx = -1;


/*
 * Load a file from FAT32 SD card and write its contents into PSRAM
 *
 * This function demonstrates how to transfer arbitrary file data from the SD card
 * (using the FAT32 file system driver) directly into external PSRAM attached to the Pico.
 * 
 * The FAT32 driver allows you to open files and read their contents in chunks. 
 * The PSRAM driver (see psram_spi.h and psram_spi.c) provides a simple API for
 * writing bytes to PSRAM at any address, so you can treat PSRAM like a large block
 * of memory for caching, buffering, or temporary data storage.
 *
 * Usage:
 *    load_file_to_psram("myfile.txt", 0x00000);
 *      - Reads "myfile.txt" from the SD card and writes it to PSRAM starting at address 0x00000.
 *
 * Notes and best practices:
 * - You can choose any starting PSRAM address, but make sure you don’t overwrite important data.
 * - This function reads the file in 256-byte blocks and writes each block sequentially.
 * - You can read back data from PSRAM using psram_read() or psram_read8/16/32, for random access.
 * - The FAT32 driver is robust, supporting long filenames and directories.
 * - PSRAM is not persistent storage; data will be lost on power-off or reset.
 * - You can use PSRAM to cache files, implement virtual memory, or buffer media for fast access.
 *
 * Example code for reading back:
 *    uint8_t buffer[256];
 *    psram_read(&psram_spi, 0x00000, buffer, sizeof(buffer));
 *
 * Both the FAT32 and PSRAM drivers are portable and efficient, making this setup suitable for
 * embedded projects where fast, temporary storage is required.
 */

void load_file_to_psram(const char *filename, uint32_t psram_addr) {
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, filename);
    if (result != FAT32_OK) {
        printf("Error opening file: %s\n", fat32_error_string(result));
        return;
    }
    uint8_t buffer[256];
    size_t bytes_read;
    uint32_t addr = psram_addr;
    while (1) {
        result = fat32_read(&file, buffer, sizeof(buffer), &bytes_read);
        if (result != FAT32_OK || bytes_read == 0) break;
        psram_write(&psram_spi, addr, buffer, bytes_read);
        addr += bytes_read;
    }
    fat32_close(&file);
    printf("Loaded %s to PSRAM at 0x%08X\n", filename, psram_addr);
}

void add_to_history(const char *command) {
    if (command[0] == '\0') return;

    strncpy(history[history_next_idx], command, MAX_COMMAND_LENGTH - 1);
    history[history_next_idx][MAX_COMMAND_LENGTH - 1] = '\0';

    history_next_idx = (history_next_idx + 1) % MAX_HISTORY;
    history_cycle_idx = -1;
}

static bool history_has_entries() {
    for (int i = 0; i < MAX_HISTORY; i++) {
        if (history[i][0] != '\0') return true;
    }
    return false;
}

void read_password(char *buffer, int maxLength) {
    int idx = 0;
    while (idx < maxLength - 1) {
        if (!keyboard_key_available()) {
            tight_loop_contents();
            continue;
        }
        int raw = keyboard_get_key();
        if (raw < 0) continue;
        unsigned char c = (unsigned char)raw;

        if (c == '\r' || c == '\n') break;
        if (c == 0x7F || c == 0x08) {
            if (idx > 0) {
                idx--;
                display_emit('\b');
                display_emit(' ');
                display_emit('\b');
            }
            continue;
        }
        if (c >= 32 && c <= 126) {
            buffer[idx++] = c;
            display_emit('*');
        }
    }
    buffer[idx] = '\0';
    display_emit('\r');
    display_emit('\n');
}

void safe_gets(char *buffer, int maxLength) {
    read_line(buffer, maxLength);
}

void command_history_cycle_up(char *buffer, int *idx) {
    if (!history_has_entries()) return;
    int original = history_cycle_idx;
    if (history_cycle_idx == -1) {
        history_cycle_idx = (history_next_idx - 1 + MAX_HISTORY) % MAX_HISTORY;
    } else {
        history_cycle_idx = (history_cycle_idx - 1 + MAX_HISTORY) % MAX_HISTORY;
    }
    while (history[history_cycle_idx][0] == '\0' &&
           history_cycle_idx != original) {
        history_cycle_idx = (history_cycle_idx - 1 + MAX_HISTORY) % MAX_HISTORY;
    }
    if (history[history_cycle_idx][0] == '\0') {
        history_cycle_idx = original;
        return;
    }
    while (*idx > 0) {
        display_emit('\b');
        display_emit(' ');
        display_emit('\b');
        (*idx)--;
    }
    strncpy(buffer, history[history_cycle_idx], MAX_COMMAND_LENGTH - 1);
    buffer[MAX_COMMAND_LENGTH - 1] = '\0';
    *idx = strlen(buffer);
    for (int i = 0; i < *idx; i++) {
        display_emit(buffer[i]);
    }
}

void command_history_cycle_down(char *buffer, int *idx) {
    if (!history_has_entries() || history_cycle_idx == -1) return;
    int original = history_cycle_idx;
    int next_idx = (history_cycle_idx + 1) % MAX_HISTORY;
    if (next_idx == history_next_idx || history[next_idx][0] == '\0') {
        while (*idx > 0) {
            display_emit('\b');
            display_emit(' ');
            display_emit('\b');
            (*idx)--;
        }
        buffer[0] = '\0';
        history_cycle_idx = -1;
        return;
    }
    history_cycle_idx = next_idx;
    while (history[history_cycle_idx][0] == '\0' &&
           history_cycle_idx != original) {
        history_cycle_idx = (history_cycle_idx + 1) % MAX_HISTORY;
        if (history_cycle_idx == history_next_idx) {
            history_cycle_idx = -1;
            buffer[0] = '\0';
            while (*idx > 0) {
                display_emit('\b');
                display_emit(' ');
                display_emit('\b');
                (*idx)--;
            }
            return;
        }
    }
    while (*idx > 0) {
        display_emit('\b');
        display_emit(' ');
        display_emit('\b');
        (*idx)--;
    }
    strncpy(buffer, history[history_cycle_idx], MAX_COMMAND_LENGTH - 1);
    buffer[MAX_COMMAND_LENGTH - 1] = '\0';
    *idx = strlen(buffer);
    for (int i = 0; i < *idx; i++) {
        display_emit(buffer[i]);
    }
}

volatile bool user_interrupt = false;

void read_line(char *buffer, int maxLength) {
    int idx = 0;
    history_cycle_idx = -1;
    buffer[0] = '\0';
    
    while (1) {
        if (!keyboard_key_available()) {
            tight_loop_contents();
            continue;
        }

        int raw = keyboard_get_key();
        if (raw < 0) continue;

        unsigned char c = (unsigned char)raw;

        if (c == '\r' || c == '\n') {
            display_emit('\r');
            display_emit('\n');
            buffer[idx] = '\0';
            return;
        }

        if ((c == 0x7F || c == 0x08) && idx > 0) {
            idx--;
            buffer[idx] = '\0';
            display_emit('\b');
            display_emit(' ');
            display_emit('\b');
            continue;
        }

        if (c == 0xB5) {
            command_history_cycle_up(buffer, &idx);
            printf("\r> ");
            for (int i = 0; i < idx; i++) {
                display_emit(buffer[i]);
            }
            continue;
        }
        if (c == 0xB6) {
            command_history_cycle_down(buffer, &idx);
            printf("\r> ");
            for (int i = 0; i < idx; i++) {
                display_emit(buffer[i]);
            }
            continue;
        }
        if (c == 0xB7) {
        }
        if (c == 0xB4) {
        }
        if (c >= 32 && c <= 126 && idx < maxLength - 1) {
            buffer[idx++] = c;
            buffer[idx] = '\0';
            display_emit(c);
            history_cycle_idx = -1;
        }
    }
}

#define MAX_NETWORKS 20
#define MAC_ADDR_LEN 6

typedef struct {
    uint8_t bssid[MAC_ADDR_LEN];
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
    uint8_t auth_mode;
} network_info_t;

static network_info_t found_networks[MAX_NETWORKS];
static volatile bool wifi_scan_complete = false;
static volatile int networks_found = 0;

static int wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        for (int i = 0; i < networks_found; i++) {
            if (memcmp(found_networks[i].bssid, result->bssid, MAC_ADDR_LEN) == 0) {
                return 0;
            }
        }

        if (networks_found < MAX_NETWORKS) {
            memcpy(found_networks[networks_found].bssid, result->bssid, MAC_ADDR_LEN);
            memcpy(found_networks[networks_found].ssid, (const char*)result->ssid, 32);
            found_networks[networks_found].ssid[32] = '\0';
            found_networks[networks_found].rssi = result->rssi;
            found_networks[networks_found].channel = result->channel;
            found_networks[networks_found].auth_mode = result->auth_mode;
            networks_found++;

            printf("\n%d. Network SSID: %s", networks_found, found_networks[networks_found-1].ssid);
            printf("\n   MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
                result->bssid[0], result->bssid[1], result->bssid[2],
                result->bssid[3], result->bssid[4], result->bssid[5]);
            printf("\n   Signal Strength: %d dBm", result->rssi);
            printf("\n   Channel: %d", result->channel);
            printf("\n   Security: ");
            uint8_t auth = result->auth_mode & 0xFF;
            switch(auth) {
                case 0:
                    printf("Open (No Security)");
                    break;
                case 1:
                    printf("WEP");
                    break;
                case 2:
                    printf("WPA-PSK");
                    break;
                case 3:
                    printf("WPA2-PSK");
                    break;
                case 4:
                    printf("WPA/WPA2 Mixed");
                    break;
                default:
                    printf("Unknown/Other");
            }
            printf("\n   Band: 2.4 GHz\n");
        }
    }
    return 0;
}

void handle_timer_interrupt(void) {
    timer_hw->intr = 1u << 0;
    timer_hw->alarm[0] += 10000;
    mini_seconds += 10;

    if (mini_seconds >= 1000) {
        mini_seconds = 0;
        seconds++;
        if (seconds >= 60) {
            seconds = 0;
            minutes++;
            if (minutes >= 60) {
                minutes = 0;
                hours = (hours + 1) % 24;
            }
        }
    }
}

void setup_timer_interrupt() {
    timer_hw->alarm[0] = timer_hw->timerawl + 10000;
    timer_hw->inte |= 1 << 0;
    int alarm0_irq = timer_hardware_alarm_get_irq_num(timer_hw, 0);
    irq_set_exclusive_handler(alarm0_irq, handle_timer_interrupt);
    irq_set_enabled(alarm0_irq, true);
}

void command_hello(void) {
    printf("Hello, %s\n", currentUsername);
}

void command_history(void) {
    printf("Command History:\n");

    int count = 0;
    for (int i = 0; i < MAX_HISTORY; ++i) {
        int index = (history_cycle_idx + i) % MAX_HISTORY;
        if (history[index][0] != '\0') {
            printf("%d: %s\n", count + 1, history[index]);
            count++;
        }
    }

    if (count == 0) {
        printf("No commands in history.\n");
    }
}

void command_tempcheck(void) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    sleep_ms(25);

    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / (1 << 12);

    float temperature_celsius = 27.0f - (voltage - 0.706f) / 0.001721f;
    float temperature_fahrenheit = temperature_celsius * 9.0f / 5.0f + 32.0f;

    printf("Temperature: %.2f°C / %.2f°F\n", temperature_celsius, temperature_fahrenheit);

    adc_set_temp_sensor_enabled(false);
}

void command_help(void) {
    printf("Available commands: hello, help, ls (list file), mk file (make file), rm file (remove file), mkdir (make directory)," 
        "rmdir (remove directory), pwd (print working directory), cd (change directory), whoami, pico, cls, passwd, usernm, time,"
        "settime, uname, memory, echo, read file, reboot, suspend, wifi scan, wifi enable, wifi disable, mv (move file)," 
        "rn (rename file), tempcheck, history\n");
}

void command_listfiles(void) {
    fat32_error_t result;

    result = fat32_open(&dir, currentWorkingDirectory);
    if (result != FAT32_OK) {
        printf("Error opening directory: %s\n", fat32_error_string(result));
        return;
    }

    bool has_entries = false;
    while (fat32_dir_read(&dir, &entry) == FAT32_OK && entry.filename[0]) {
        if (strcmp(entry.filename, "credentials_data.txt") == 0) {
            continue;
        }
        
        has_entries = true;
        if (entry.attr & FAT32_ATTR_DIRECTORY) {
            printf("[DIR] %s\n", entry.filename);
        } else {
            printf("[FILE] %s (%lu bytes)\n", entry.filename, (unsigned long)entry.size);
        }
        sleep_ms(10);
    }

    if (!has_entries) {
        printf("Directory is empty\n");
    }

    fat32_close(&dir);
}

void command_createfile(const char *fullCommand) {
    fat32_file_t file;
    const char *filename = fullCommand + 7;
    while (*filename == ' ') filename++;
    
    char filepath[FAT32_MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s%s", currentWorkingDirectory, filename);
    
    fat32_error_t result = fat32_create(&file, filepath);
    if (result == FAT32_ERROR_FILE_EXISTS) {
        printf("Error: File '%s' already exists\n", filename);
        return;
    } else if (result != FAT32_OK) {
        printf("Error creating file\n");
        return;
    }

    printf("File '%s' created successfully.\n", filename);
    fat32_close(&file);
}

void command_deletefile(const char *fullCommand) {
    const char *filename = fullCommand + 7;
    while (*filename == ' ') filename++;
    
    char filepath[FAT32_MAX_PATH_LEN];
    snprintf(filepath, sizeof(filepath), "%s%s", currentWorkingDirectory, filename);
    
    fat32_file_t file;
    if (fat32_open(&file, filepath) != FAT32_OK) {
        printf("Error: File '%s' not found\n", filename);
        return;
    }
    fat32_close(&file);

    fat32_error_t result = fat32_delete(filepath);
    if (result != FAT32_OK) {
        printf("Error deleting file: %s\n", fat32_error_string(result));
        return;
    }

    printf("File '%s' deleted successfully.\n", filename);
}

void command_createdir(const char *fullCommand) {
    fat32_file_t dir;
    const char *dirname = fullCommand + 5;
    while (*dirname == ' ') dirname++;
    
    char dirpath[FAT32_MAX_PATH_LEN];
    snprintf(dirpath, sizeof(dirpath), "%s%s", currentWorkingDirectory, dirname);
    
    fat32_error_t result = fat32_dir_create(&dir, dirpath);
    if (result != FAT32_OK) {
        if (result == FAT32_ERROR_FILE_EXISTS) {
            printf("Error: Directory '%s' already exists\n", dirname);
            return;
        }
        printf("Error creating directory: %s\n", fat32_error_string(result));
        return;
    }

    printf("Directory '%s' created successfully.\n", dirname);
    fat32_close(&dir);
}

void command_deletedir(const char *fullCommand) {
    const char *dirname = fullCommand + 5;
    while (*dirname == ' ') dirname++;
    
    char dirpath[FAT32_MAX_PATH_LEN];
    snprintf(dirpath, sizeof(dirpath), "%s%s", currentWorkingDirectory, dirname);
    
    fat32_file_t dir;
    if (fat32_open(&dir, dirpath) != FAT32_OK) {
        printf("Error: Directory '%s' not found\n", dirname);
        return;
    }

    fat32_entry_t entry;
    while (fat32_dir_read(&dir, &entry) == FAT32_OK && entry.filename[0]) {
        if (strcmp(entry.filename, ".") != 0 && strcmp(entry.filename, "..") != 0) {
            printf("Error: Directory '%s' is not empty\n", dirname);
            fat32_close(&dir);
            return;
        }
    }
    fat32_close(&dir);

    fat32_error_t result = fat32_delete(dirpath);
    if (result != FAT32_OK) {
        printf("Error deleting directory: %s\n", fat32_error_string(result));
        return;
    }

    printf("Directory '%s' deleted successfully.\n", dirname);
}

void command_printdirectory(void) {
    printf("%s\n", currentWorkingDirectory);
}

void command_changedirectory(const char *fullCommand) {
    const char *dirname = fullCommand;
    char newpath[FAT32_MAX_PATH_LEN];
    snprintf(newpath, sizeof(newpath), "%s%s", currentWorkingDirectory, dirname);
    fat32_file_t dir;
    fat32_error_t result = fat32_open(&dir, newpath);
    if (result == FAT32_OK) {
        fat32_close(&dir);
        strncpy(currentWorkingDirectory, newpath, FAT32_MAX_PATH_LEN - 1);
        currentWorkingDirectory[FAT32_MAX_PATH_LEN - 1] = '\0';
    } else {
        result = fat32_dir_create(&dir, newpath);
        if (result == FAT32_OK) {
            printf("Directory '%s' created successfully.\n", dirname);
            fat32_close(&dir);
            strncpy(currentWorkingDirectory, newpath, FAT32_MAX_PATH_LEN - 1);
            currentWorkingDirectory[FAT32_MAX_PATH_LEN - 1] = '\0';
        } else {
            printf("Error: Directory '%s' not found and could not be created.\n", dirname);
        }
    }
}

void command_whoami(void) {
    printf("%s\n", currentUsername);
}

void command_pico(void) {
    printf("Raspberry Pi Pico 2W (RP2350) specs: \n\n");
    printf("CPU: Dual-core Arm Cortex-M33 / RISC-V Hazard3 @ 150 MHz\n");
    printf("RAM: 520 KB SRAM\n");
    printf("Flash: 4 MB QSPI external flash\n");
    printf("Wireless: 2.4 GHz 802.11n Wi-Fi (CYW43439), Bluetooth 5.2\n");
    printf("GPIO and Interfaces: 26 GPIO (4 ADC), 2xUART/SPI/I²C, 24 PWM, 12 PIO\n");
    printf("Power: 1.8-5.5V input, buck-boost SMPS\n");
    printf("USB: USB 1.1 device/host\n");
    printf("Size/Form Factor: 51x21mm, 40-pin header, SWD\n");
}

void command_cls(void) {
    lcd_clear_screen();
    lcd_set_background(RGB(0, 0, 0));
    lcd_set_foreground(RGB(255, 255, 255));
}

void command_passwd(void) {
    failedLoginAttempts = 0;
change_password:
    printf("Old Password: ");
    fflush(stdout);
    isTypingAllowed = true;
    read_password(enteredPassword, MAX_PASSWORD_LENGTH - 1);
    isTypingAllowed = false;
    failedLoginAttempts++;
    if (strcmp(enteredPassword, currentPassword) == 0) {
        printf("New Password: ");
        fflush(stdout);
        isTypingAllowed = true;
        read_password(currentPassword, MAX_PASSWORD_LENGTH - 1);
        isTypingAllowed = false;

        fat32_file_t file;
        fat32_error_t result;
        
        fat32_close(&file);
        
        if (fat32_open(&file, "credentials_data.txt") != FAT32_OK) {
            if (fat32_create(&file, "credentials_data.txt") != FAT32_OK) {
                printf("Error accessing credentials file\n");
                return;
            }
            char template_str[] = "Username:              \nPassword:              \n";
            fat32_write(&file, template_str, strlen(template_str), NULL);
            fat32_close(&file);
            
            if (fat32_open(&file, "credentials_data.txt") != FAT32_OK) {
                printf("Error reopening credentials file\n");
                return;
            }
        }

        if (fat32_seek(&file, 28) != FAT32_OK) {
            printf("Error seeking in credentials file\n");
            fat32_close(&file);
            return;
        }

        char padded_password[15];
        memset(padded_password, ' ', 14);
        size_t pass_len = strlen(currentPassword);
        if (pass_len > 14) pass_len = 14;
        memcpy(padded_password, currentPassword, pass_len);
        padded_password[14] = '\0';

        result = fat32_write(&file, padded_password, 14, NULL);
        fat32_close(&file);

        if (result != FAT32_OK) {
            printf("Error writing to credentials file\n");
            return;
        }

        printf("Password changed successfully!\n");
    } else {
        if (failedLoginAttempts < 5) {
            printf("Incorrect. Try again.\n");
            goto change_password;
        } else {
            printf("Too many attempts!\n");
        }
    }
}

void command_usernm(void) {
    printf("New Username: ");
    fflush(stdout);
    isTypingAllowed = true;
    safe_gets(currentUsername, MAX_USERNAME_LENGTH - 1);
    isTypingAllowed = false;

    fat32_file_t file;
    const char* template_str = "Username:              \nPassword:              \n";
    
    if (fat32_open(&file, "credentials_data.txt") != FAT32_OK) {
        if (fat32_create(&file, "credentials_data.txt") != FAT32_OK) {
            printf("Error creating credentials file\n");
            return;
        }
        fat32_write(&file, template_str, strlen(template_str), NULL);
        fat32_close(&file);
        if (fat32_open(&file, "credentials_data.txt") != FAT32_OK) {
            printf("Error opening credentials file\n");
            return;
        }
    }
    
    if (fat32_seek(&file, 9) != FAT32_OK) {
        printf("Error seeking in file\n");
        fat32_close(&file);
        return;
    }
    
    char padded_username[15];
    memset(padded_username, ' ', 14);
    size_t username_len = strlen(currentUsername);
    if (username_len > 14) username_len = 14;
    memcpy(padded_username, currentUsername, username_len);
    padded_username[14] = '\0';
    
    if (fat32_write(&file, padded_username, 14, NULL) != FAT32_OK) {
        printf("Error writing to credentials file\n");
        fat32_close(&file);
        return;
    }
    
    fat32_close(&file);
    printf("Username has been changed successfully!\n");
}

void command_time(void) {
    printf("Current time: %d:%d\n", hours, minutes);
}

void command_settime(void) {
    char hours_str[3];
    char minutes_str[3];
    int new_hours = 0;
    int new_minutes = 0;

    printf("Hours (00-23): ");
    fflush(stdout);
    isTypingAllowed = true;
    safe_gets(hours_str, 3);
    isTypingAllowed = false;
    new_hours = atoi(hours_str);

    if (new_hours < 0 || new_hours > 23) {
        printf("Invalid hour. Must be between 00 and 23.\n");
        return;
    }

    printf("Minutes (00-59): ");
    fflush(stdout);
    isTypingAllowed = true;
    safe_gets(minutes_str, 3);
    isTypingAllowed = false;
    new_minutes = atoi(minutes_str);

    if (new_minutes < 0 || new_minutes > 59) {
        printf("Invalid minute. Must be between 00 and 59.\n");
        return;
    }

    hours = new_hours;
    minutes = new_minutes;
    printf("System time updated to %02d:%02d\n", hours, minutes);
}

void command_uname(void) {
    printf("Astralixi Kernel Version Beta\n");
    printf("Made by Astrox");
}

void command_reboot(void) {
    watchdog_reboot(0, 0, 0);
    while (true);
}

void command_suspend(void) {
    printf("Suspending. Hit enter/return to return \n");
    sleep_ms(500);

    lcd_display_off();
    cyw43_arch_deinit();

    char suspendchar = keyboard_get_key();
    if (suspendchar == '\r' || suspendchar == '\n') {
        lcd_display_on();
    }
}

void command_memory(void) {
    const size_t total_ram = 520 * 1024;
    extern char _end;
    extern char __StackTop;
    char *heap_start = &_end;
    char *stack_top = &__StackTop;
    size_t available = (size_t)(stack_top - heap_start);
    printf("Available memory: %lu bytes\n", (unsigned long)available);
    printf("Used memory: %lu bytes\n", (unsigned long) total_ram - available);
    printf("Total memory: %lu bytes\n", (unsigned long)total_ram);
}

void command_echo(const char *fullCommand) {
    const char *message = fullCommand + 4;
    while (*message == ' ') message++;
    printf("%s\n", message);
}

void command_open_and_read_file(const char* filename) {
    if (strcmp(filename, "credentials_data.txt") == 0) {
        printf("Error: Access denied\n");
        return;
    }
    
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, filename);
    if (result != FAT32_OK) {
        printf("Error opening file: %s\n", fat32_error_string(result));
        return;
    }
    char buffer[512];
    size_t bytes_read;
    while (true) {
        result = fat32_read(&file, buffer, sizeof(buffer) - 1, &bytes_read);
        if (result != FAT32_OK) {
            printf("Error reading file: %s\n", fat32_error_string(result));
            break;
        }
        if (bytes_read == 0) break;
        buffer[bytes_read] = '\0';
        char* line = buffer;
        while (*line) {
            char* end = strchr(line, '\n');
            if (end) {
                *end = '\0';
                printf("%s\n", line);
                line = end + 1;
            } else {
                printf("%s\n", line);
                break;
            }
        }
    }
    fat32_close(&file);
}

void command_wifi_scan(void) {
    printf("Starting WiFi network scan...\n");
    printf("This may take up to 15 seconds...\n\n");
    
    networks_found = 0;
    memset(found_networks, 0, sizeof(found_networks));
    wifi_scan_complete = false;

    cyw43_arch_enable_sta_mode();

    cyw43_wifi_scan_options_t scan_options = {0};

    int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, wifi_scan_result);
    if (err != 0) {
        printf("Failed to start WiFi scan (error: %d)\n", err);
        return;
    }

    absolute_time_t timeout = make_timeout_time_ms(15000);
    char progress[] = "|/-\\";
    int progress_idx = 0;
    
    while (!wifi_scan_complete && !time_reached(timeout)) {
        printf("\rScanning %c", progress[progress_idx]);
        progress_idx = (progress_idx + 1) % 4;
        
        if (!cyw43_wifi_scan_active(&cyw43_state)) {
            wifi_scan_complete = true;
        }
        
        tight_loop_contents();
        sleep_ms(100);
    }
    printf("\r          \r");

    if (!wifi_scan_complete) {
        printf("\nScan timed out.\n");
    } else if (networks_found == 0) {
        printf("\nNo wireless networks found.\n");
    } else {
        printf("\nScan complete! Found %d network%s.\n", 
               networks_found, networks_found == 1 ? "" : "s");
    }
}

void command_disable_wifi(void) {
    cyw43_arch_deinit();
    sleep_ms(50);
    printf("\r Wifi disabled! \n");
}

void command_enable_wifi(void) {
    cyw43_arch_init();
    sleep_ms(50);
    printf("\r Wifi enabled! \n");
}

void command_rename_file(const char *fullCommand) {
    const char *old_name = fullCommand + 2;
    while (*old_name == ' ') old_name++;
    
    const char *space = strchr(old_name, ' ');
    if (!space) {
        printf("Usage: rn <oldname> <newname>\n");
        return;
    }
    
    const char *new_name = space + 1;
    while (*new_name == ' ') new_name++;

    char old_path[FAT32_MAX_PATH_LEN];
    char new_path[FAT32_MAX_PATH_LEN];
    
    size_t prefix_len = strlen(currentWorkingDirectory);
    size_t name_len = space - old_name;
    
    if (prefix_len + name_len >= FAT32_MAX_PATH_LEN) {
        printf("Error: Path too long\n");
        return;
    }
    
    memcpy(old_path, currentWorkingDirectory, prefix_len);
    memcpy(old_path + prefix_len, old_name, name_len);
    old_path[prefix_len + name_len] = '\0';
    
    if (prefix_len + strlen(new_name) >= FAT32_MAX_PATH_LEN) {
        printf("Error: New path too long\n");
        return;
    }
    memcpy(new_path, currentWorkingDirectory, prefix_len);
    strcpy(new_path + prefix_len, new_name);
    
    fat32_file_t file;
    if (fat32_open(&file, old_path) != FAT32_OK) {
        printf("Error: File not found\n");
        return;
    }
    fat32_close(&file);
    
    if (fat32_rename(old_path, new_path) != FAT32_OK) {
        printf("Error renaming file\n");
        return;
    }
    
    printf("File renamed successfully\n");
}

void command_move_file(const char *fullCommand) {
    const char *src = fullCommand + 3;
    while (*src == ' ') src++;
    
    const char *space = strchr(src, ' ');
    if (!space) {
        printf("Usage: mv <source> <destdir>\n");
        return;
    }
    
    char src_path[FAT32_MAX_PATH_LEN], dest_path[FAT32_MAX_PATH_LEN];
    size_t src_len = space - src;
    const char *dest = space + 1;
    while (*dest == ' ') dest++;
    
    size_t prefix_len = strlen(currentWorkingDirectory);
    if (prefix_len + src_len >= FAT32_MAX_PATH_LEN) {
        printf("Error: Source path too long\n");
        return;
    }
    
    memcpy(src_path, currentWorkingDirectory, prefix_len);
    memcpy(src_path + prefix_len, src, src_len);
    src_path[prefix_len + src_len] = '\0';
    
    size_t dest_len = strlen(dest);
    size_t full_dest_len = prefix_len + dest_len + src_len + 2;
    if (full_dest_len > FAT32_MAX_PATH_LEN) {
        printf("Error: Destination path too long\n");
        return;
    }
    
    memcpy(dest_path, currentWorkingDirectory, prefix_len);
    strcpy(dest_path + prefix_len, dest);
    strcat(dest_path, "/");
    memcpy(dest_path + prefix_len + dest_len + 1, src, src_len);
    dest_path[full_dest_len - 1] = '\0';
    
    fat32_file_t file;
    fat32_error_t result = fat32_open(&file, src_path);
    if (result != FAT32_OK) {
        printf("Error: Source file not found\n");
        return;
    }
    fat32_close(&file);

    char dest_dir[FAT32_MAX_PATH_LEN];
    char *last_slash;
    strncpy(dest_dir, dest_path, FAT32_MAX_PATH_LEN);
    last_slash = strrchr(dest_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        fat32_dir_create(&file, dest_dir);
    }

    if (fat32_rename(src_path, dest_path) == FAT32_OK) {
        printf("File moved successfully\n");
    } else {
        printf("Error moving file\n");
    }
}

void execute_command(const char *command) {
    if (strcmp(command, "hello") == 0) {  
        command_hello();
    } else if (strcmp(command, "help") == 0) {  
        command_help();
    } else if (strcmp(command, "whoami") == 0) {  
        command_whoami();
    } else if (strcmp(command, "passwd") == 0) {   
        command_passwd();
    } else if (strcmp(command, "time") == 0) {  
        command_time();
    } else if (strcmp(command, "settime") == 0) {  
        command_settime();
    } else if (strcmp(command, "uname") == 0) {  
        command_uname();
    } else if (strcmp(command, "usernm") == 0) {  
        command_usernm();
    } else if (strcmp(command, "clear") == 0 || strcmp(command, "cls") == 0) {  
        command_cls();
    } else if (strncmp(command, "echo", 4) == 0) {  
        command_echo(command);
    } else if (strcmp(command, "pico") == 0) {  
        command_pico();
    } else if (strcmp(command, "pwd") == 0) {  
        command_printdirectory();
    } else if (strcmp(command, "ls") == 0 || strcmp(command, "list files") == 0) {  
        command_listfiles();
    } else if (strncmp(command, "read file", 9) == 0) {  
        const char* filename = command + 10;
        while (*filename == ' ') filename++;
        command_open_and_read_file(filename);
    } else if (strncmp(command, "cd", 2) == 0) {    
        const char* dirname = command + 3;
        while (*dirname == ' ') dirname++;
        command_changedirectory(dirname);
    } else if (strncmp(command, "rn", 2) == 0) {   
        command_rename_file(command);
    } else if (strncmp(command, "mv", 2) == 0) {  
        command_move_file(command);
    } else if (strncmp(command, "mk file", 7) == 0) {  
        command_createfile(command);
    } else if (strncmp(command, "rm file", 7) == 0) {  
        command_deletefile(command);
    } else if (strncmp(command, "mkdir", 5) == 0) {  
        command_createdir(command);
    } else if (strncmp(command, "rmdir", 5) == 0) {  
        command_deletedir(command);
    } else if (strcmp(command, "memory") == 0) {  
        command_memory();
    } else if (strcmp(command, "reboot") == 0) {  
        printf("Rebooting... ");
        sleep_ms(250);
        command_reboot();
    } else if (strcmp(command, "suspend") == 0) {  
        command_suspend();
    } else if (strcmp(command, "wifi scan") == 0) {   
        command_wifi_scan();
    } else if (strcmp(command, "history") == 0) {  
        command_history();
    } else if (strcmp(command, "tempcheck") == 0) {  
        command_tempcheck();
    } else if (strcmp(command, "wifi enable") == 0) {  
        command_enable_wifi();
    } else if (strcmp(command, "wifi disable") == 0) {  
        command_disable_wifi();
    } else {
        printf("Command Not Found.\n");
    }
}

void kernel() {
    picocalc_init();
    sleep_ms(500);

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return;
    }
}

void load_credentials(void) {
    fat32_init();
    if (!fat32_is_ready()) {
        strncpy(currentUsername, "admin", MAX_USERNAME_LENGTH - 1);
        strncpy(currentPassword, "admin", MAX_PASSWORD_LENGTH - 1);
        currentUsername[MAX_USERNAME_LENGTH - 1] = '\0';
        currentPassword[MAX_PASSWORD_LENGTH - 1] = '\0';
        return;
    }

    fat32_file_t file;
    const char* template_str = "Username:              \nPassword:              \n";
    char buffer[43];
    size_t bytes_read;

    fat32_error_t result = fat32_open(&file, "credentials_data.txt");
    if (result != FAT32_OK) {
        result = fat32_create(&file, "credentials_data.txt");
        if (result != FAT32_OK) {
            strncpy(currentUsername, "admin", MAX_USERNAME_LENGTH - 1);
            strncpy(currentPassword, "admin", MAX_PASSWORD_LENGTH - 1);
            currentUsername[MAX_USERNAME_LENGTH - 1] = '\0';
            currentPassword[MAX_PASSWORD_LENGTH - 1] = '\0';
            return;
        }
        fat32_write(&file, template_str, strlen(template_str), NULL);
        fat32_close(&file);
        
        strncpy(currentUsername, "admin", MAX_USERNAME_LENGTH - 1);
        strncpy(currentPassword, "admin", MAX_PASSWORD_LENGTH - 1);
        currentUsername[MAX_USERNAME_LENGTH - 1] = '\0';
        currentPassword[MAX_PASSWORD_LENGTH - 1] = '\0';
        return;
    }

    result = fat32_read(&file, buffer, sizeof(buffer)-1, &bytes_read);
    fat32_close(&file);

    if (result != FAT32_OK || bytes_read < 42) {
        strncpy(currentUsername, "admin", MAX_USERNAME_LENGTH - 1);
        strncpy(currentPassword, "admin", MAX_PASSWORD_LENGTH - 1);
        currentUsername[MAX_USERNAME_LENGTH - 1] = '\0';
        currentPassword[MAX_PASSWORD_LENGTH - 1] = '\0';
        return;
    }

    char temp[15];
    memcpy(temp, buffer + 9, 14);
    temp[14] = '\0';
    for (int i = 13; i >= 0 && temp[i] == ' '; i--) {
        temp[i] = '\0';
    }
    if (temp[0] != '\0') {
        strncpy(currentUsername, temp, MAX_USERNAME_LENGTH - 1);
        currentUsername[MAX_USERNAME_LENGTH - 1] = '\0';
    }

    memcpy(temp, buffer + 28, 14);
    temp[14] = '\0';
    for (int i = 13; i >= 0 && temp[i] == ' '; i--) {
        temp[i] = '\0';
    }
    if (temp[0] != '\0') {
        strncpy(currentPassword, temp, MAX_PASSWORD_LENGTH - 1);
        currentPassword[MAX_PASSWORD_LENGTH - 1] = '\0';
    }
}

int main() {
    picocalc_init();
    sleep_ms(500);

    lcd_clear_screen();
    lcd_set_background(RGB(0, 0, 0));
    lcd_set_foreground(RGB(255, 255, 255));

    printf("\rAstralixi OS Booting on PicoCalc...\n");
    sleep_ms(250);

    sd_init();
    sd_error_t err = sd_card_init();
    if (err != SD_OK) {
        printf("\rSD init failed: %s\n\n", sd_error_string(err));
        sleep_ms(1000);
    }

    // Initialize PSRAM (using PIO 0, state machine 0, fudge enabled)
    psram_spi = psram_spi_init_clkdiv(pio0, 0, 1.0, true);

    load_credentials();

    if (cyw43_arch_init()) {
        printf("\rWi-Fi init failed\n");
    }

    setup_timer_interrupt();

    printf("\rWelcome to Astralixi OS!\n");
    printf("\r--------------------------\n\n");
    sleep_ms(250);

Login_Sequence:
    printf("\rUsername: ");
    read_line(enteredUsername, MAX_USERNAME_LENGTH);

    printf("\rPassword: ");
    read_password(enteredPassword, MAX_PASSWORD_LENGTH);  

    sleep_ms(500);
    failedLoginAttempts++;

    if (strcmp(enteredUsername, currentUsername) == 0 &&
        strcmp(enteredPassword, currentPassword) == 0) {
        printf("\rLogin Successful!\n");
    } else {
        printf("\rIncorrect Login!\n");
	sleep_ms(1000);
        if (failedLoginAttempts < 5) {
            goto Login_Sequence;
        } else {
            printf("\rToo many login attempts!\n");
            sleep_ms(1000);
            return 0;
        }
    }

    void init_history() {
        memset(history, 0, sizeof(history));
        history_cycle_idx = 0;
        history_cycle_idx = -1;
    }

    memset(history, 0, sizeof(history));

    while (1) {
        printf("\r> ");

        char userCommand[MAX_COMMAND_LENGTH];

        read_line(userCommand, MAX_COMMAND_LENGTH);

        if (strcmp(userCommand, "exit") == 0) {
            printf("YOU WILL HAVE TO RESTART YOUR PICOCALC TO GET OUT OF THIS EXIT LOOP\n");
            sleep_ms(500);
            break;
        }

        add_to_history(userCommand);  
        execute_command(userCommand);

        sleep_ms(20);
        mini_seconds++;

        while (mini_seconds >= 50) {
            seconds++;
            mini_seconds -= 50;
        }

        while (seconds >= 60) {
            minutes++;
            seconds -= 60;
        }

        while (minutes >= 60) {
            hours++;
            minutes -= 60;
        }

        while (hours >= 24) {
            hours -= 24;
        }
    }

    return 0;
}
