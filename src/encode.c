#include <stdlib.h> //for memcpy
#include "encode.h"

size_t frame_message(
    encode_state* s, 
    const char* message, 
    size_t message_len, 
    char* data, 
    size_t data_len) {
  //TODO: handle data_len being too small by sending only some of the message
  if (data_len < message_len + (message_len/s->frame_len + 1) * 
      (s->start_framing_len + s->end_framing_len)) {
    return 0;
  }
  char* data_moving = data;
  for (size_t i = 0; i < message_len; i += s->frame_len) {
    memcpy(data_moving, s->start_framing, start_framing_len);
    data_moving += s->start_framing_len;
    memcpy(data, &message[i], 
      ((message_len - i) < s->frame_len) ? message_len - i : s->frame_len);
    data_moving += ((message_len - i) < s->frame_len) ? message_len - i : s->frame_len; //FIXME
    memcpy(data, s->start_framing, end_framing_len);
    data_moving += s->end_framing_len;
  }
  return data_moving - data;
}

size_t encode_message(
    encode_state* s,
    const char* data, 
    size_t data_len, 
    sample_t* symbols, 
    size_t symbols_len) {
  if (symbols_len < 4*data_len) {
    return 0;
  }
  for (size_t i = 0; i < data_len; i++) {
    //it would be better to gray-code these, but we don't for compatibility
    symbols[4*i + 0] = (data[i] & 0x03);
    symbols[4*i + 1] = (data[i] & 0x0C) >> 2;
    symbols[4*i + 2] = (data[i] & 0x30) >> 4;
    symbols[4*i + 3] = (data[i] & 0xC0) >> 6;
  }
  return 4*data_len;
}
  