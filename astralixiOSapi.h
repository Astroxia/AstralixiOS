#ifndef ASTRALIXI_OS_API_H
#define ASTRALIXI_OS_API_H
#include <stdbool.h>
#include "pico/stdlib.h"
#include "drivers/fat32.h"
// Constants
#define MAX_COMMAND_LENGTH 100
#define MAX_USERNAME_LENGTH 15
#define MAX_PASSWORD_LENGTH 15
#define FAT32_MAX_PATH_LEN 256
// Global variables
extern char currentUsername[MAX_USERNAME_LENGTH];
extern char currentPassword[MAX_PASSWORD_LENGTH];
extern char currentWorkingDirectory[FAT32_MAX_PATH_LEN];
extern int failedLoginAttempts;
extern char currentTime[6]; // HH:MM + null terminator
extern volatile bool isTypingAllowed;
// Keyboard input functions
void keyboard_input_callback(void);
int custom_getchar(void);
void safe_gets(char *buffer, int maxLength);
// Command functions
void command_hello(void);
void command_exit(void);
void command_help(void);
void command_listfiles(void);
void command_createfile(const char *fullCommand);
void command_deletefile(const char *fullCommand);
void command_createdir(const char *fullCommand);
void command_deletedir(const char *fullCommand);
void command_printdirectory(void);
void command_changedirectory(const char *fullCommand);
void command_whoami(void);
void command_pico(void);
void command_cls(void);
void command_passwd(void);
void command_usernm(void);
void command_time(void);
void command_settime(void);
void command_uname(void);
void command_memory(void);
void command_echo(const char *fullCommand);
void command_open_and_read_file(const char* filename);
// Kernel functions
void kernel(void);
// Command execution
void execute_command(const char *command);
#endif // ASTRALIXI_OS_API_H