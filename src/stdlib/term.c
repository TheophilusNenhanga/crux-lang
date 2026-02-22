#include "stdlib/term.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "stdlib/event.h"

#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#define KEY_REPEAT_THRESHOLD_MS 50

static bool terminal_initialized = false;
static ObjectKey *last_key = NULL;
static uint64_t last_key_time = 0;

#ifdef _WIN32
static DWORD original_input_mode = 0;
static DWORD original_output_mode = 0;
static HANDLE hstdin = INVALID_HANDLE_VALUE;
static HANDLE hstdout = INVALID_HANDLE_VALUE;
#else
static struct termios original_termios;
static int original_flags = -1;
#endif

static uint64_t get_timestamp_ms(void)
{
#ifdef _WIN32
	return GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

static void write_stdout(const char *str, size_t len)
{
#ifdef _WIN32
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str, (DWORD)len, NULL, NULL);
#else
	write(STDOUT_FILENO, str, len);
#endif
}

static bool get_terminal_size(int *cols, int *rows)
{
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (!GetConsoleScreenBufferInfo(hstdout, &csbi)) {
		return false;
	}
	*cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	*rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	return true;
#else
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
		return false;
	}
	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return true;
#endif
}

static bool enable_raw_mode(void)
{
#ifdef _WIN32
	hstdin = GetStdHandle(STD_INPUT_HANDLE);
	hstdout = GetStdHandle(STD_OUTPUT_HANDLE);

	if (hstdin == INVALID_HANDLE_VALUE || hstdout == INVALID_HANDLE_VALUE) {
		return false;
	}

	if (!GetConsoleMode(hstdin, &original_input_mode)) {
		return false;
	}
	if (!GetConsoleMode(hstdout, &original_output_mode)) {
		return false;
	}

	DWORD new_input_mode = original_input_mode;
	new_input_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
			    ENABLE_PROCESSED_INPUT);
	new_input_mode |= ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;

	DWORD new_output_mode = original_output_mode;
	new_output_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

	if (!SetConsoleMode(hstdin, new_input_mode)) {
		return false;
	}
	if (!SetConsoleMode(hstdout, new_output_mode)) {
		SetConsoleMode(hstdin, original_input_mode);
		return false;
	}

	return true;
#else
	if (!isatty(STDIN_FILENO)) {
		return false;
	}

	if (tcgetattr(STDIN_FILENO, &original_termios) < 0) {
		return false;
	}

	original_flags = fcntl(STDIN_FILENO, F_GETFL, 0);

	struct termios raw = original_termios;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
		return false;
	}

	fcntl(STDIN_FILENO, F_SETFL, original_flags | O_NONBLOCK);

	return true;
#endif
}

static void disable_raw_mode(void)
{
#ifdef _WIN32
	if (hstdin != INVALID_HANDLE_VALUE) {
		SetConsoleMode(hstdin, original_input_mode);
	}
	if (hstdout != INVALID_HANDLE_VALUE) {
		SetConsoleMode(hstdout, original_output_mode);
	}
#else
	if (original_flags >= 0) {
		fcntl(STDIN_FILENO, F_SETFL, original_flags);
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
	}
#endif
}

static void enable_mouse(void)
{
	write_stdout("\x1b[?1000h", 8);
	write_stdout("\x1b[?1006h", 8);
}

static void disable_mouse(void)
{
	write_stdout("\x1b[?1000l", 8);
	write_stdout("\x1b[?1006l", 8);
}

static void enable_alt_screen(void)
{
	write_stdout("\x1b[?1049h", 8);
}

static void disable_alt_screen(void)
{
	write_stdout("\x1b[?1049l", 8);
}

Value term_init_function(VM *vm, const Value *args)
{
	(void)args;

	if (terminal_initialized) {
		return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
	}

	if (!enable_raw_mode()) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to enable raw mode.", IO);
	}

	enable_alt_screen();
	enable_mouse();

	terminal_initialized = true;

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

Value term_exit_function(VM *vm, const Value *args)
{
	(void)vm;
	(void)args;

	if (terminal_initialized) {
		disable_mouse();
		disable_alt_screen();
		disable_raw_mode();
		terminal_initialized = false;
	}

	return NIL_VAL;
}

Value term_size_function(VM *vm, const Value *args)
{
	(void)args;

	int cols, rows;
	if (!get_terminal_size(&cols, &rows)) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to get terminal size.",
					  IO);
	}

	ObjectArray *arr = new_array(vm, 2);
	push(vm->current_module_record, OBJECT_VAL(arr));
	arr->values[0] = INT_VAL(cols);
	arr->values[1] = INT_VAL(rows);
	arr->size = 2;

	ObjectResult *result = new_ok_result(vm, OBJECT_VAL(arr));
	pop(vm->current_module_record);

	return OBJECT_VAL(result);
}

Value term_width_function(VM *vm, const Value *args)
{
	(void)args;

	int cols, rows;
	if (!get_terminal_size(&cols, &rows)) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to get terminal size.",
					  IO);
	}

	return OBJECT_VAL(new_ok_result(vm, INT_VAL(cols)));
}

Value term_height_function(VM *vm, const Value *args)
{
	(void)args;

	int cols, rows;
	if (!get_terminal_size(&cols, &rows)) {
		return MAKE_GC_SAFE_ERROR(vm, "Failed to get terminal size.",
					  IO);
	}

	return OBJECT_VAL(new_ok_result(vm, INT_VAL(rows)));
}

Value term_mouse_enable_function(VM *vm, const Value *args)
{
	(void)vm;
	(void)args;
	enable_mouse();
	return NIL_VAL;
}

Value term_mouse_disable_function(VM *vm, const Value *args)
{
	(void)vm;
	(void)args;
	disable_mouse();
	return NIL_VAL;
}

#ifndef _WIN32
static ssize_t read_stdin(void *buf, size_t count)
{
	return read(STDIN_FILENO, buf, count);
}
#else
static DWORD read_stdin(void *buf, DWORD count)
{
	DWORD read_count = 0;
	ReadFile(hstdin, buf, count, &read_count, NULL);
	return read_count;
}
#endif

static int read_byte(void)
{
	unsigned char c;
#ifndef _WIN32
	ssize_t n = read_stdin(&c, 1);
#else
	DWORD n = read_stdin(&c, 1);
#endif
	return n > 0 ? c : -1;
}

static bool keys_equal(ObjectKey *a, ObjectKey *b)
{
	if (!a || !b)
		return false;
	if (a->key_type != b->key_type)
		return false;
	if (a->key_type == KEY_CHAR || a->key_type == KEY_CTRL ||
	    a->key_type == KEY_ALT) {
		return a->character == b->character;
	}
	return true;
}

static ObjectEvent *create_key_event(VM *vm, ObjectKey *key, bool is_down,
				     bool shift, bool ctrl, bool alt,
				     bool repeat)
{
	ObjectString *type_str = copy_string(vm, is_down ? "keydown" : "keyup",
					     is_down ? 7 : 5);
	push(vm->current_module_record, OBJECT_VAL(type_str));

	ObjectString *source_str = copy_string(vm, "term", 4);
	push(vm->current_module_record, OBJECT_VAL(source_str));

	ObjectTable *data = new_table(vm, 8, vm->current_module_record);
	push(vm->current_module_record, OBJECT_VAL(data));

	ObjectString *key_key = copy_string(vm, "key", 3);
	push(vm->current_module_record, OBJECT_VAL(key_key));
	object_table_set(vm, data, OBJECT_VAL(key_key), OBJECT_VAL(key));
	pop(vm->current_module_record);

	ObjectString *shift_key = copy_string(vm, "shift", 5);
	push(vm->current_module_record, OBJECT_VAL(shift_key));
	object_table_set(vm, data, OBJECT_VAL(shift_key), BOOL_VAL(shift));
	pop(vm->current_module_record);

	ObjectString *ctrl_key = copy_string(vm, "ctrl", 4);
	push(vm->current_module_record, OBJECT_VAL(ctrl_key));
	object_table_set(vm, data, OBJECT_VAL(ctrl_key), BOOL_VAL(ctrl));
	pop(vm->current_module_record);

	ObjectString *alt_key = copy_string(vm, "alt", 3);
	push(vm->current_module_record, OBJECT_VAL(alt_key));
	object_table_set(vm, data, OBJECT_VAL(alt_key), BOOL_VAL(alt));
	pop(vm->current_module_record);

	ObjectString *repeat_key = copy_string(vm, "repeat", 6);
	push(vm->current_module_record, OBJECT_VAL(repeat_key));
	object_table_set(vm, data, OBJECT_VAL(repeat_key), BOOL_VAL(repeat));
	pop(vm->current_module_record);

	uint64_t timestamp = get_timestamp_ms();

	ObjectEvent *event = new_event(vm, type_str, source_str, data,
				       timestamp);

	pop(vm->current_module_record);
	pop(vm->current_module_record);
	pop(vm->current_module_record);

	return event;
}

static ObjectEvent *create_mouse_event(VM *vm, const char *type, int x, int y,
				       int button, bool shift, bool ctrl,
				       bool alt)
{
	ObjectString *type_str = copy_string(vm, type, strlen(type));
	push(vm->current_module_record, OBJECT_VAL(type_str));

	ObjectString *source_str = copy_string(vm, "term", 4);
	push(vm->current_module_record, OBJECT_VAL(source_str));

	ObjectTable *data = new_table(vm, 8, vm->current_module_record);
	push(vm->current_module_record, OBJECT_VAL(data));

	ObjectString *x_key = copy_string(vm, "x", 1);
	push(vm->current_module_record, OBJECT_VAL(x_key));
	object_table_set(vm, data, OBJECT_VAL(x_key), INT_VAL(x));
	pop(vm->current_module_record);

	ObjectString *y_key = copy_string(vm, "y", 1);
	push(vm->current_module_record, OBJECT_VAL(y_key));
	object_table_set(vm, data, OBJECT_VAL(y_key), INT_VAL(y));
	pop(vm->current_module_record);

	ObjectString *button_key = copy_string(vm, "button", 6);
	push(vm->current_module_record, OBJECT_VAL(button_key));
	object_table_set(vm, data, OBJECT_VAL(button_key), INT_VAL(button));
	pop(vm->current_module_record);

	ObjectString *shift_key = copy_string(vm, "shift", 5);
	push(vm->current_module_record, OBJECT_VAL(shift_key));
	object_table_set(vm, data, OBJECT_VAL(shift_key), BOOL_VAL(shift));
	pop(vm->current_module_record);

	ObjectString *ctrl_key = copy_string(vm, "ctrl", 4);
	push(vm->current_module_record, OBJECT_VAL(ctrl_key));
	object_table_set(vm, data, OBJECT_VAL(ctrl_key), BOOL_VAL(ctrl));
	pop(vm->current_module_record);

	ObjectString *alt_key = copy_string(vm, "alt", 3);
	push(vm->current_module_record, OBJECT_VAL(alt_key));
	object_table_set(vm, data, OBJECT_VAL(alt_key), BOOL_VAL(alt));
	pop(vm->current_module_record);

	uint64_t timestamp = get_timestamp_ms();

	ObjectEvent *event = new_event(vm, type_str, source_str, data,
				       timestamp);

	pop(vm->current_module_record);
	pop(vm->current_module_record);
	pop(vm->current_module_record);

	return event;
}

static ObjectEvent *create_resize_event(VM *vm, int width, int height)
{
	ObjectString *type_str = copy_string(vm, "resize", 6);
	push(vm->current_module_record, OBJECT_VAL(type_str));

	ObjectString *source_str = copy_string(vm, "term", 4);
	push(vm->current_module_record, OBJECT_VAL(source_str));

	ObjectTable *data = new_table(vm, 4, vm->current_module_record);
	push(vm->current_module_record, OBJECT_VAL(data));

	ObjectString *width_key = copy_string(vm, "width", 5);
	push(vm->current_module_record, OBJECT_VAL(width_key));
	object_table_set(vm, data, OBJECT_VAL(width_key), INT_VAL(width));
	pop(vm->current_module_record);

	ObjectString *height_key = copy_string(vm, "height", 6);
	push(vm->current_module_record, OBJECT_VAL(height_key));
	object_table_set(vm, data, OBJECT_VAL(height_key), INT_VAL(height));
	pop(vm->current_module_record);

	uint64_t timestamp = get_timestamp_ms();

	ObjectEvent *event = new_event(vm, type_str, source_str, data,
				       timestamp);

	pop(vm->current_module_record);
	pop(vm->current_module_record);
	pop(vm->current_module_record);

	return event;
}

static ObjectKey *parse_escape_sequence(VM *vm, bool *ctrl, bool *alt,
					bool *shift)
{
	int c = read_byte();
	if (c < 0)
		return NULL;

	if (c == '[') {
		int c2 = read_byte();
		if (c2 < 0)
			return NULL;

		int params[8] = {0};
		int param_count = 0;
		char final_char = 0;

		while (c2 >= '0' && c2 <= '9') {
			int val = 0;
			while (c2 >= '0' && c2 <= '9') {
				val = val * 10 + (c2 - '0');
				c2 = read_byte();
				if (c2 < 0)
					return NULL;
			}
			params[param_count++] = val;
			if (c2 == ';') {
				c2 = read_byte();
				if (c2 < 0)
					return NULL;
			}
		}
		final_char = c2;

		*shift = false;
		*ctrl = false;
		*alt = false;

		if (param_count >= 2 &&
		    (final_char == 'A' || final_char == 'B' ||
		     final_char == 'C' || final_char == 'D')) {
			int mods = params[1];
			if (mods >= 1) {
				mods -= 1;
				*shift = (mods & 1) != 0;
				*alt = (mods & 2) != 0;
				*ctrl = (mods & 4) != 0;
			}
		}

		switch (final_char) {
		case 'A':
			return new_key(vm, KEY_UP, '\0');
		case 'B':
			return new_key(vm, KEY_DOWN, '\0');
		case 'C':
			return new_key(vm, KEY_RIGHT, '\0');
		case 'D':
			return new_key(vm, KEY_LEFT, '\0');
		case 'H':
			return new_key(vm, KEY_HOME, '\0');
		case 'F':
			return new_key(vm, KEY_END, '\0');
		case 'Z':
			*shift = true;
			return new_key(vm, KEY_TAB, '\0');
		}

		if (final_char == '~') {
			switch (params[0]) {
			case 1:
				return new_key(vm, KEY_HOME, '\0');
			case 2:
				return new_key(vm, KEY_INSERT, '\0');
			case 3:
				return new_key(vm, KEY_DELETE, '\0');
			case 4:
				return new_key(vm, KEY_END, '\0');
			case 5:
				return new_key(vm, KEY_PAGEUP, '\0');
			case 6:
				return new_key(vm, KEY_PAGEDOWN, '\0');
			case 11:
				return new_key(vm, KEY_F1, '\0');
			case 12:
				return new_key(vm, KEY_F2, '\0');
			case 13:
				return new_key(vm, KEY_F3, '\0');
			case 14:
				return new_key(vm, KEY_F4, '\0');
			case 15:
				return new_key(vm, KEY_F5, '\0');
			case 17:
				return new_key(vm, KEY_F6, '\0');
			case 18:
				return new_key(vm, KEY_F7, '\0');
			case 19:
				return new_key(vm, KEY_F8, '\0');
			case 20:
				return new_key(vm, KEY_F9, '\0');
			case 21:
				return new_key(vm, KEY_F10, '\0');
			case 23:
				return new_key(vm, KEY_F11, '\0');
			case 24:
				return new_key(vm, KEY_F12, '\0');
			}
		}

		if (final_char == 'M' || final_char == 'm') {
			return NULL;
		}

		if (final_char == '<') {
			return NULL;
		}

		return NULL;
	}

	if (c == 'O') {
		int c2 = read_byte();
		if (c2 < 0)
			return NULL;

		switch (c2) {
		case 'P':
			return new_key(vm, KEY_F1, '\0');
		case 'Q':
			return new_key(vm, KEY_F2, '\0');
		case 'R':
			return new_key(vm, KEY_F3, '\0');
		case 'S':
			return new_key(vm, KEY_F4, '\0');
		case 'H':
			return new_key(vm, KEY_HOME, '\0');
		case 'F':
			return new_key(vm, KEY_END, '\0');
		}
		return NULL;
	}

	return NULL;
}

static ObjectEvent *parse_mouse_sgr(VM *vm, int button_code, int x, int y,
				    bool is_press)
{
	bool shift = false;
	bool ctrl = false;
	bool alt = false;

	int button = 0;

	if (button_code >= 0 && button_code <= 2) {
		button = button_code + 1;
	} else if (button_code == 64) {
		ObjectEvent *event = create_mouse_event(vm, "mousescroll", x, y,
							0, shift, ctrl, alt);
		push(vm->current_module_record, OBJECT_VAL(event));

		ObjectString *dx_key = copy_string(vm, "dx", 2);
		push(vm->current_module_record, OBJECT_VAL(dx_key));
		object_table_set(vm, event->data, OBJECT_VAL(dx_key),
				 INT_VAL(0));
		pop(vm->current_module_record);

		ObjectString *dy_key = copy_string(vm, "dy", 2);
		push(vm->current_module_record, OBJECT_VAL(dy_key));
		object_table_set(vm, event->data, OBJECT_VAL(dy_key),
				 INT_VAL(-1));
		pop(vm->current_module_record);

		pop(vm->current_module_record);
		return event;
	} else if (button_code == 65) {
		ObjectEvent *event = create_mouse_event(vm, "mousescroll", x, y,
							0, shift, ctrl, alt);
		push(vm->current_module_record, OBJECT_VAL(event));

		ObjectString *dx_key = copy_string(vm, "dx", 2);
		push(vm->current_module_record, OBJECT_VAL(dx_key));
		object_table_set(vm, event->data, OBJECT_VAL(dx_key),
				 INT_VAL(0));
		pop(vm->current_module_record);

		ObjectString *dy_key = copy_string(vm, "dy", 2);
		push(vm->current_module_record, OBJECT_VAL(dy_key));
		object_table_set(vm, event->data, OBJECT_VAL(dy_key),
				 INT_VAL(1));
		pop(vm->current_module_record);

		pop(vm->current_module_record);
		return event;
	}

	return create_mouse_event(vm, is_press ? "mousedown" : "mouseup", x, y,
				  button, shift, ctrl, alt);
}

static ObjectEvent *read_single_event(VM *vm)
{
	int c = read_byte();
	if (c < 0)
		return NULL;

	if (c == '\x1b') {
		bool ctrl = false, alt = false, shift = false;
		ObjectKey *key = parse_escape_sequence(vm, &ctrl, &alt, &shift);

		if (key) {
			alt = true;

			bool is_repeat = false;
			uint64_t now = get_timestamp_ms();
			if (keys_equal(key, last_key) &&
			    (now - last_key_time) < KEY_REPEAT_THRESHOLD_MS) {
				is_repeat = true;
			}
			last_key = key;
			last_key_time = now;

			return create_key_event(vm, key, true, shift, ctrl, alt,
						is_repeat);
		}

		int c2 = read_byte();
		if (c2 < 0) {
			ObjectKey *esc_key = new_key(vm, KEY_ESCAPE, '\0');
			return create_key_event(vm, esc_key, true, false, false,
						false, false);
		}

		if (c2 == '[') {
			int c3 = read_byte();
			if (c3 == '<') {
				int button_code = 0;
				int x = 0, y = 0;

				int d = read_byte();
				while (d >= '0' && d <= '9') {
					button_code = button_code * 10 +
						      (d - '0');
					d = read_byte();
				}

				d = read_byte();
				while (d >= '0' && d <= '9') {
					x = x * 10 + (d - '0');
					d = read_byte();
				}

				d = read_byte();
				while (d >= '0' && d <= '9') {
					y = y * 10 + (d - '0');
					d = read_byte();
				}

				bool is_press = (d == 'M');

				return parse_mouse_sgr(vm, button_code, x, y,
						       is_press);
			}
		}

		return NULL;
	}

	if (c == '\r' || c == '\n') {
		ObjectKey *key = new_key(vm, KEY_ENTER, '\0');
		bool is_repeat = false;
		uint64_t now = get_timestamp_ms();
		if (keys_equal(key, last_key) &&
		    (now - last_key_time) < KEY_REPEAT_THRESHOLD_MS) {
			is_repeat = true;
		}
		last_key = key;
		last_key_time = now;
		return create_key_event(vm, key, true, false, false, false,
					is_repeat);
	}

	if (c == '\t') {
		ObjectKey *key = new_key(vm, KEY_TAB, '\0');
		bool is_repeat = false;
		uint64_t now = get_timestamp_ms();
		if (keys_equal(key, last_key) &&
		    (now - last_key_time) < KEY_REPEAT_THRESHOLD_MS) {
			is_repeat = true;
		}
		last_key = key;
		last_key_time = now;
		return create_key_event(vm, key, true, false, false, false,
					is_repeat);
	}

	if (c == 127 || c == 8) {
		ObjectKey *key = new_key(vm, KEY_BACKSPACE, '\0');
		bool is_repeat = false;
		uint64_t now = get_timestamp_ms();
		if (keys_equal(key, last_key) &&
		    (now - last_key_time) < KEY_REPEAT_THRESHOLD_MS) {
			is_repeat = true;
		}
		last_key = key;
		last_key_time = now;
		return create_key_event(vm, key, true, false, false, false,
					is_repeat);
	}

	if (c >= 1 && c <= 26) {
		ObjectKey *key = new_key(vm, KEY_CTRL, 'a' + c - 1);
		bool is_repeat = false;
		uint64_t now = get_timestamp_ms();
		if (keys_equal(key, last_key) &&
		    (now - last_key_time) < KEY_REPEAT_THRESHOLD_MS) {
			is_repeat = true;
		}
		last_key = key;
		last_key_time = now;
		return create_key_event(vm, key, true, false, true, false,
					is_repeat);
	}

	if (c >= 32 && c < 127) {
		ObjectKey *key = new_key(vm, KEY_CHAR, (char)c);
		bool is_repeat = false;
		uint64_t now = get_timestamp_ms();
		if (keys_equal(key, last_key) &&
		    (now - last_key_time) < KEY_REPEAT_THRESHOLD_MS) {
			is_repeat = true;
		}
		last_key = key;
		last_key_time = now;
		return create_key_event(vm, key, true, false, false, false,
					is_repeat);
	}

	return NULL;
}

Value term_read_event_function(VM *vm, const Value *args)
{
	(void)args;

	while (true) {
		ObjectEvent *event = read_single_event(vm);
		if (event) {
			return OBJECT_VAL(new_ok_result(vm, OBJECT_VAL(event)));
		}

#ifdef _WIN32
		Sleep(10);
#else
		usleep(10000);
#endif
	}
}

Value term_read_event_timeout_function(VM *vm, const Value *args)
{
	if (!IS_INT(args[0])) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected int argument.", TYPE);
	}

	int timeout_ms = AS_INT(args[0]);
	uint64_t start = get_timestamp_ms();

	while ((int)(get_timestamp_ms() - start) < timeout_ms) {
		ObjectEvent *event = read_single_event(vm);
		if (event) {
			return OBJECT_VAL(new_ok_result(vm, OBJECT_VAL(event)));
		}

#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}

Value term_poll_event_function(VM *vm, const Value *args)
{
	(void)args;

	ObjectEvent *event = read_single_event(vm);
	if (event) {
		return OBJECT_VAL(new_ok_result(vm, OBJECT_VAL(event)));
	}

	return OBJECT_VAL(new_ok_result(vm, NIL_VAL));
}
