#ifndef IMV_CONSOLE
#define IMV_CONSOLE

#include <stdbool.h>

struct imv_console;

/* Create a console instance */
struct imv_console *imv_console_create(void);

/* Clean up a console */
void imv_console_free(struct imv_console *console);

/* Set the callback to be invoked when a command to run by the console */
typedef void (*imv_console_callback)(const char *command, void *data);
void imv_console_set_command_callback(struct imv_console *console,
    imv_console_callback callback, void *data);

/* Returns true if console is still active (i.e. user hasn't hit enter/escape yet */
bool imv_console_is_active(struct imv_console *console);

/* Mark console as activated until user exits or submits a command */
void imv_console_activate(struct imv_console *console);

/* Pass text input to the console */
void imv_console_input(struct imv_console *console, const char *text);

/* Pass a key input to the console. Returns true if consumed. If so,
 * do not also send input text to the console.
 */
bool imv_console_key(struct imv_console *console, const char *key);

/* What is the console prompt's current text? */
const char *imv_console_prompt(struct imv_console *console);

/* What is the output history of the console? */
const char *imv_console_backlog(struct imv_console *console);

/* Write some text to the console's backlog */
void imv_console_write(struct imv_console *console, const char *text);

/* Add a tab-completion template. If the users hits tab, the rest of the
 * command will be suggested. If multiple matches, tab will cycle through them.
 */
void imv_console_add_completion(struct imv_console *console, const char *template);

#endif
