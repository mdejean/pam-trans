#ifndef UI_H
#define UI_H

typedef enum ui_entry_type {
  UI_ENTRY_INT = 1,
  UI_ENTRY_FLOAT,
  UI_ENTRY_STRING
} ui_entry_type;

typedef enum ui_button {
  UI_BUTTON_UP = 1,
  UI_BUTTON_DOWN,
  UI_BUTTON_LEFT,
  UI_BUTTON_RIGHT,
  UI_BUTTON_ENTER,
  UI_BUTTON_CANCEL
} ui_button;

//return true to consume the button press
typedef bool (*ui_callback)(ui_button button, void* user_data);

typedef struct ui_entry {
  ui_entry_type type;
  void* target;
  ui_callback callback;
  void* user_data;
} ui_entry;

bool ui_init(const ui_entry* entries, size_t count);

void ui_tick();

#endif