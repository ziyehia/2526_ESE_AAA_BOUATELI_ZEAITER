/**
 * @file shell.c
 * @brief Implementation file for the shell module.
 *
 * This file contains the implementation of the shell module, which provides a command-line interface.
 */

#include "user_interface/shell.h"

h_shell_t hshell1;

/**
 * @brief Transmit function for shell driver using UART2
 */



/**
 * @brief Checks if a character is valid for a shell command.
 *
 * This function checks if a character is valid for a shell command. Valid characters include alphanumeric characters and spaces.
 *
 * @param c The character to check.
 * @return 1 if the character is valid, 0 otherwise.
 */
static int is_character_valid(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == ' ') || (c == '=') || (c == '_');
}

static int is_string_valid(const char* str)
{
    if (str == NULL || *str == '\0') {
        return 0;
    }
    for (; *str != '\0'; str++) {
        if (!is_character_valid(*str)) {
            return 0;
        }
    }
    return 1;
}

/* snprintf returns the requested length, which may exceed the buffer. */
static void shell_transmit_buffer(h_shell_t* h_shell, int size)
{
    if (size > 0) {
        const uint16_t length = (size < SHELL_PRINT_BUFFER_SIZE)
            ? (uint16_t)size : SHELL_PRINT_BUFFER_SIZE - 1U;
        h_shell->drv.transmit(h_shell->print_buffer, length);
    }
}

/**
 * @brief Help command implementation.
 *
 * This function is the implementation of the help command. It displays the list of available shell commands and their descriptions.
 *
 * @param h_shell The pointer to the shell instance.
 * @param argc The number of command arguments.
 * @param argv The array of command arguments.
 * @return 0 on success.
 */
static int sh_help(h_shell_t* h_shell, int argc, char** argv)
{
	int i, size;
    (void)argc;
    (void)argv;
	size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE, "Code \t | Description \r\n");
	shell_transmit_buffer(h_shell, size);
	size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE, "----------------------\r\n");
	shell_transmit_buffer(h_shell, size);

	for (i = 0; i < h_shell->func_list_size; i++){
		size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE, "%s \t | %s\r\n", h_shell->func_list[i].string_func_code, h_shell->func_list[i].description);
		shell_transmit_buffer(h_shell, size);
	}
	return 0;
}

static int sh_test_list(h_shell_t* h_shell, int argc, char** argv)
{
	int size;
	for(int arg=0; arg<argc; arg++){
		size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE, "Arg %d \t %s\r\n", arg, argv[arg]);
		shell_transmit_buffer(h_shell, size);
	}
	return 0;
}




/**
 * @brief Initializes the shell instance.
 *
 * This function initializes the shell instance by setting up the internal data structures and registering the help command.
 *
 * @param h_shell The pointer to the shell instance.
 */
void shell_init(h_shell_t* h_shell)
{
	int size = 0;

	h_shell->func_list_size = 0;
    h_shell->cmd_buffer_index = 0;
    h_shell->discard_line = 0;
    h_shell->last_was_cr = 0;
    h_shell->cmd_buffer[0] = '\0';

	size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE, "\r\n=> Monsieur Shell v0.2.2 without FreeRTOS <=\r\n");
	shell_transmit_buffer(h_shell, size);
	h_shell->drv.transmit(PROMPT, sizeof(PROMPT) - 1U);

	shell_add(h_shell, "help", sh_help, "Help");
	shell_add(h_shell, "test", sh_test_list, "Test list");
}

/**
 * @brief Adds a shell command to the instance.
 *
 * This function adds a shell command to the shell instance.
 *
 * @param h_shell The pointer to the shell instance.
 * @param c The character trigger for the command.
 * @param pfunc Pointer to the function implementing the command.
 * @param description The description of the command.
 * @return 0 on success, or a negative error code on failure.
 */
int shell_add(h_shell_t* h_shell, char* string_func_code, shell_func_pointer_t pfunc, char* description)
{
	if(is_string_valid(string_func_code))
	{
		if (h_shell->func_list_size < SHELL_FUNC_LIST_MAX_SIZE)
		{
			h_shell->func_list[h_shell->func_list_size].string_func_code = string_func_code;
			h_shell->func_list[h_shell->func_list_size].func = pfunc;
			h_shell->func_list[h_shell->func_list_size].description = description;
			h_shell->func_list_size++;
			return 0;
		}
	}
	return -1;
}

/**
 * @brief Executes a shell command.
 *
 * This function executes a shell command based on the input buffer.
 *
 * @param h_shell The pointer to the shell instance.
 * @param buf The input buffer containing the command.
 * @return 0 on success, or a negative error code on failure.
 */
static int shell_exec(h_shell_t* h_shell, char* buf)
{
    int i;
    int argc = 0;
    char* argv[SHELL_ARGC_MAX];
    char* p = buf;
    while (*p != '\0') {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (argc == SHELL_ARGC_MAX) {
            h_shell->drv.transmit("Too many arguments\r\n", 20);
            return -1;
        }
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ') {
            p++;
        }
        if (*p != '\0') {
            *p++ = '\0';
        }
    }
    if (argc == 0) {
        return 0;
    }

	for (i = 0; i < h_shell->func_list_size; i++)
	{
		if(strcmp(h_shell->func_list[i].string_func_code, argv[0])==0)
		{

			return h_shell->func_list[i].func(h_shell, argc, argv);
		}
	}

	int size;
	size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE, "%s : no such command\r\n", argv[0]);
	shell_transmit_buffer(h_shell, size);
	return -1;
}

/* Discard until the next complete line, even if later bytes look valid. */
void shell_discard_line(h_shell_t* h_shell)
{
    h_shell->cmd_buffer_index = 0;
    h_shell->discard_line = 1;
    h_shell->last_was_cr = 0;
}

/* Consume at most one queued byte; never wait for a complete command. */
int shell_run(h_shell_t* h_shell)
{
    char c;
    if (h_shell->drv.receive(&c, 1) != 1U) {
        return 0;
    }
    if (c == '\n' && h_shell->last_was_cr != 0U) {
        h_shell->last_was_cr = 0;
        return 0;
    }
    h_shell->last_was_cr = (c == '\r');

    if (c == '\r' || c == '\n') {
        int result = 0;
        h_shell->drv.transmit("\r\n", 2);
        if (h_shell->discard_line != 0U) {
            h_shell->drv.transmit("Invalid or too long command\r\n",
                sizeof("Invalid or too long command\r\n") - 1U);
            result = -1;
        } else {
            h_shell->cmd_buffer[h_shell->cmd_buffer_index] = '\0';
            result = shell_exec(h_shell, h_shell->cmd_buffer);
        }
        h_shell->cmd_buffer_index = 0;
        h_shell->discard_line = 0;
        h_shell->cmd_buffer[0] = '\0';
        h_shell->drv.transmit(PROMPT, sizeof(PROMPT) - 1U);
        return result;
    }
    if (h_shell->discard_line != 0U) {
        return 0;
    }
    if (c == '\b' || c == 127) {
        if (h_shell->cmd_buffer_index > 0U) {
            h_shell->cmd_buffer[--h_shell->cmd_buffer_index] = '\0';
            h_shell->drv.transmit("\b \b", 3);
        }
        return 0;
    }
    if (!is_character_valid(c)
        || h_shell->cmd_buffer_index >= SHELL_CMD_BUFFER_SIZE - 1U) {
        shell_discard_line(h_shell);
        return -1;
    }
    h_shell->cmd_buffer[h_shell->cmd_buffer_index++] = c;
    h_shell->drv.transmit(&c, 1);
    return 0;
}
