#ifndef UI_H
#define UI_H

#include <stddef.h>
#include <stdint.h>

#define UI_MAX_NAME_LENGTH 20
#define UI_MAX_VALUE_LENGTH 20

typedef enum ui_entry_type {
  UI_ENTRY_UINT32 = 1,
  UI_ENTRY_FLOAT,
  UI_ENTRY_STRING
} ui_entry_type;

typedef enum ui_button {
  UI_BUTTON_UP = 0x01,
  UI_BUTTON_DOWN = 0x02,
  UI_BUTTON_LEFT = 0x04,
  UI_BUTTON_RIGHT = 0x08,
  UI_BUTTON_ENTER = 0x10,
  UI_BUTTON_CANCEL = 0x20
} ui_button;

//forward declaration because callback is in and takes struct
typedef struct ui_entry ui_entry;

//return true to consume the button press
typedef bool (*ui_callback)(const ui_entry* entry, ui_button button);

struct ui_entry {
  ui_entry_type type;
  const char* name;
  void* value;
  ui_callback callback;
  void* user_data;
};

bool ui_init(const ui_entry* entries, size_t count);

//call to check input state & update display
void ui_tick();

//a callback that does nothing
bool ui_callback_none(const ui_entry* entry, ui_button button);

void ui_set_status(uint8_t o);

uint8_t ui_get_status();

#endif
