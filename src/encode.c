#include <stdlib.h> //for memcpy
#include "encode.h"

bool encode_init(size_t frame_len,  
    const uint8_t* start_framing;
    size_t start_framing_len,
    const uint8_t* end_framing,
    size_t end_framing_len) {
  s->frame_len         = frame_len;
  s->start_framing     = start_framing; 
  s->start_framing_len = start_framing_len;   
  s->end_framing       = end_framing; 
  s->end_framing_len   = end_framing_len;  
}

size_t frame_message(
    encode_state* s, 
    const uint8_t* message, 
    size_t message_len, 
    uint8_t* data, 
    size_t data_len) {
  //TODO: handle data_len being too small by sending only some of the message
  if (data_len < message_len + (message_len/s->frame_len + 1) * 
      (s->start_framing_len + s->end_framing_len)) {
    return 0;
  }
  uint8_t* data_moving = data;
  for (size_t i = 0; i < message_len; i += s->frame_len) {
    if (s->start_framing_len > 0) {
      memcpy(data_moving, s->start_framing, s->start_framing_len);
      data_moving += s->start_framing_len;
    }
    
    memcpy(data_moving, &message[i], 
      ((message_len - i) < s->frame_len) ? message_len - i : s->frame_len);
    data_moving += ((message_len - i) < s->frame_len) ? message_len - i : s->frame_len; //FIXME
    
    if (s->end_framing_len > 0) {
      memcpy(data_moving, s->end_framing, s->end_framing_len);
      data_moving += s->end_framing_len;
    }
  }
  return data_moving - data;
}

size_t encode_message(
    encode_state* s,
    const uint8_t* data, 
    size_t data_len, 
    sample_t* symbols, 
    size_t symbols_len) {
  if (symbols_len < 4*data_len) {
    return 0;
  }
  for (size_t i = 0; i < data_len; i++) {
    //it would be better to gray-code these, but we don't for compatibility
    symbols[4*i + 0] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * (data[i] & 0x03);
    symbols[4*i + 1] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * ((data[i] & 0x0C) >> 2);
    symbols[4*i + 2] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * ((data[i] & 0x30) >> 4);
    symbols[4*i + 3] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * ((data[i] & 0xC0) >> 6);
  }
  return 4*data_len;
}
  