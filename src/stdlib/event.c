#include "stdlib/event.h"
#include "garbage_collector.h"
#include "object.h"
#include "panic.h"
#include "value.h"
#include "vm.h"

#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <stdint.h>
#endif

static ObjectKey *create_key(VM *vm, KeyType key_type, char character)
{
	return new_key(vm, key_type, character);
}

/**
 * Creates a new key object with the character.
 * arg0 -> string
 * Returns Result<Key>
 */
Value key_char_function(VM *vm, const Value *args)
{
	ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected non-empty string.",
					  VALUE);
	}
	ObjectKey *key = create_key(vm, KEY_CHAR, str->chars[0]);
	return MAKE_GC_SAFE_RESULT(vm, key);
}

Value key_enter_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_ENTER, '\0'));
}

Value key_tab_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_TAB, '\0'));
}

Value key_backspace_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_BACKSPACE, '\0'));
}

Value key_delete_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_DELETE, '\0'));
}

Value key_escape_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_ESCAPE, '\0'));
}

Value key_up_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_UP, '\0'));
}

Value key_down_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_DOWN, '\0'));
}

Value key_left_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_LEFT, '\0'));
}

Value key_right_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_RIGHT, '\0'));
}

Value key_home_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_HOME, '\0'));
}

Value key_end_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_END, '\0'));
}

Value key_page_up_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_PAGEUP, '\0'));
}

Value key_page_down_function(VM *vm, const Value *args)
{
	(void)args;
	return OBJECT_VAL(create_key(vm, KEY_PAGEDOWN, '\0'));
}

Value key_f_function(VM *vm, const Value *args)
{
	int n = AS_INT(args[0]);
	if (n < 1 || n > 12) {
		return MAKE_GC_SAFE_ERROR(vm, "F-key must be 1-12.", VALUE);
	}
	ObjectKey *key = create_key(vm, (KeyType)(KEY_F1 + n - 1), '\0');
	return MAKE_GC_SAFE_RESULT(vm, key);
}

Value key_ctrl_function(VM *vm, const Value *args)
{
	ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected non-empty string.",
					  VALUE);
	}
	ObjectKey *key = create_key(vm, KEY_CTRL, str->chars[0]);
	return MAKE_GC_SAFE_RESULT(vm, key);
}

Value key_alt_function(VM *vm, const Value *args)
{
	ObjectString *str = AS_CRUX_STRING(args[0]);
	if (str->length == 0) {
		return MAKE_GC_SAFE_ERROR(vm, "Expected non-empty string.",
					  VALUE);
	}
	ObjectKey *key = create_key(vm, KEY_ALT, str->chars[0]);
	return MAKE_GC_SAFE_RESULT(vm, key);
}

Value key_is_char_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectKey *key = AS_CRUX_KEY(args[0]);
	return BOOL_VAL(key->key_type == KEY_CHAR);
}

Value key_is_arrow_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectKey *key = AS_CRUX_KEY(args[0]);
	return BOOL_VAL(key->key_type >= KEY_UP && key->key_type <= KEY_RIGHT);
}

Value key_is_function_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectKey *key = AS_CRUX_KEY(args[0]);
	return BOOL_VAL(key->key_type >= KEY_F1 && key->key_type <= KEY_F12);
}

Value key_is_modifier_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectKey *key = AS_CRUX_KEY(args[0]);
	return BOOL_VAL(key->key_type == KEY_CTRL || key->key_type == KEY_ALT);
}

Value key_char_method(VM *vm, const Value *args)
{
	ObjectKey *key = AS_CRUX_KEY(args[0]);
	if (key->key_type != KEY_CHAR) {
		return NIL_VAL;
	}
	char c[2] = {key->character, '\0'};
	return OBJECT_VAL(copy_string(vm, c, 1));
}

Value key_name_method(VM *vm, const Value *args)
{
	ObjectKey *key = AS_CRUX_KEY(args[0]);
	const char *name;

	switch (key->key_type) {
	case KEY_CHAR:
		name = (char[]){key->character, '\0'};
		break;
	case KEY_ENTER:
		name = "Enter";
		break;
	case KEY_TAB:
		name = "Tab";
		break;
	case KEY_BACKSPACE:
		name = "Backspace";
		break;
	case KEY_DELETE:
		name = "Delete";
		break;
	case KEY_INSERT:
		name = "Insert";
		break;
	case KEY_ESCAPE:
		name = "Escape";
		break;
	case KEY_UP:
		name = "Up";
		break;
	case KEY_DOWN:
		name = "Down";
		break;
	case KEY_LEFT:
		name = "Left";
		break;
	case KEY_RIGHT:
		name = "Right";
		break;
	case KEY_HOME:
		name = "Home";
		break;
	case KEY_END:
		name = "End";
		break;
	case KEY_PAGEUP:
		name = "PageUp";
		break;
	case KEY_PAGEDOWN:
		name = "PageDown";
		break;
	case KEY_F1:
		name = "F1";
		break;
	case KEY_F2:
		name = "F2";
		break;
	case KEY_F3:
		name = "F3";
		break;
	case KEY_F4:
		name = "F4";
		break;
	case KEY_F5:
		name = "F5";
		break;
	case KEY_F6:
		name = "F6";
		break;
	case KEY_F7:
		name = "F7";
		break;
	case KEY_F8:
		name = "F8";
		break;
	case KEY_F9:
		name = "F9";
		break;
	case KEY_F10:
		name = "F10";
		break;
	case KEY_F11:
		name = "F11";
		break;
	case KEY_F12:
		name = "F12";
		break;
	case KEY_CTRL:
		name = (char[]){'C', 't', 'r', 'l', '+', key->character, '\0'};
		break;
	case KEY_ALT:
		name = (char[]){'A', 'l', 't', '+', key->character, '\0'};
		break;
	default:
		name = "Unknown";
	}

	if (key->key_type == KEY_CHAR || key->key_type == KEY_CTRL ||
	    key->key_type == KEY_ALT) {
		static char buffer[16];
		if (key->key_type == KEY_CHAR) {
			snprintf(buffer, sizeof(buffer), "%c", key->character);
		} else if (key->key_type == KEY_CTRL) {
			snprintf(buffer, sizeof(buffer), "Ctrl+%c",
				 key->character);
		} else {
			snprintf(buffer, sizeof(buffer), "Alt+%c",
				 key->character);
		}
		return OBJECT_VAL(copy_string(vm, buffer, strlen(buffer)));
	}

	return OBJECT_VAL(copy_string(vm, name, strlen(name)));
}

Value key_equals_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectKey *a = AS_CRUX_KEY(args[0]);
	ObjectKey *b = AS_CRUX_KEY(args[1]);

	if (a->key_type != b->key_type) {
		return BOOL_VAL(false);
	}
	if (a->key_type == KEY_CHAR || a->key_type == KEY_CTRL ||
	    a->key_type == KEY_ALT) {
		return BOOL_VAL(a->character == b->character);
	}
	return BOOL_VAL(true);
}

Value event_new_function(VM *vm, const Value *args)
{
	ObjectString *type = AS_CRUX_STRING(args[0]);
	ObjectString *source = AS_CRUX_STRING(args[1]);
	ObjectTable *data = AS_CRUX_TABLE(args[2]);

	uint64_t timestamp = 0;
#ifdef _WIN32
	timestamp = GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	timestamp = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif

	ObjectEvent *event = new_event(vm, type, source, data, timestamp);
	return OBJECT_VAL(event);
}

Value event_type_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return OBJECT_VAL(event->type);
}

Value event_source_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return OBJECT_VAL(event->source);
}

Value event_data_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return OBJECT_VAL(event->data);
}

Value event_timestamp_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return FLOAT_VAL((double)event->timestamp);
}

static bool string_equals(ObjectString *a, const char *b)
{
	size_t len = strlen(b);
	return a->length == len && memcmp(a->chars, b, len) == 0;
}

Value event_is_key_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return BOOL_VAL(string_equals(event->type, "keydown") ||
			string_equals(event->type, "keyup"));
}

Value event_is_keydown_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return BOOL_VAL(string_equals(event->type, "keydown"));
}

Value event_is_keyup_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return BOOL_VAL(string_equals(event->type, "keyup"));
}

Value event_is_mouse_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return BOOL_VAL(string_equals(event->type, "mousedown") ||
			string_equals(event->type, "mouseup") ||
			string_equals(event->type, "mousemove") ||
			string_equals(event->type, "mousescroll"));
}

Value event_is_resize_method(VM *vm, const Value *args)
{
	(void)vm;
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);
	return BOOL_VAL(string_equals(event->type, "resize"));
}

Value event_key_method(VM *vm, const Value *args)
{
	ObjectEvent *event = AS_CRUX_EVENT(args[0]);

	Value key_val;
	ObjectString *key_str = copy_string(vm, "key", 3);
	push(vm->current_module_record, OBJECT_VAL(key_str));
	bool found = object_table_get(event->data->entries, event->data->size,
				      event->data->capacity,
				      OBJECT_VAL(key_str), &key_val);
	pop(vm->current_module_record);

	if (!found || !IS_CRUX_KEY(key_val)) {
		return NIL_VAL;
	}
	return key_val;
}
