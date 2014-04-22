#include <string.h> //for memcpy
#include "encode.h"

bool encode_init(encode_state* s)
  if (s->frame_len < 1) return false;
  if (s->start_framing) return false;  
  if (s->end_framing) return false;
  return true;
}

size_t frame_message(
    encode_state* s, 
    const uint8_t* message, 
    size_t message_len, 
    uint8_t* data, 
    size_t data_len,
    size_t* data_used) {
  
  if (data_len < s->frame_len + s->start_framing_len + s->end_framing_len)) {
    return 0;
  }
  size_t data_pos = 0;
  
  size_t next_frame_len = s->frame_len;
  if (message_len < s->frame_len) {
    next_frame_len = message_len;
  }
  
  for (size_t i = 0; 
      i < message_len 
      && data_pos 
        + s->start_framing_len 
        + next_frame_len 
        + s->end_framing_len
        < data_len;
      i += s->frame_len) {
    //add the start framing (if enabled)
    if (s->start_framing_len > 0) {
      memcpy(&data[data_pos], s->start_framing, s->start_framing_len);
      data_pos += s->start_framing_len;
    }
    
    //add the message
    memcpy(&data[data_pos], &message[i], next_frame_len);
    data_pos += next_frame_len;
    
    //add the end framing (if enabled)
    if (s->end_framing_len > 0) {
      memcpy(&data[data_pos], s->end_framing, s->end_framing_len);
      data_pos += s->end_framing_len;
    }
    
    //calculate the length of the next frame (how far are we from the end?)
    if ((message_len - i) < (2 * s->frame_len)) {
      next_frame_len = message_len - i;
    }
  }
  if (message_consumed != NULL) { 
    *data_consumed = data_moving;
  }
  return i;
}

size_t encode_data(
    encode_state* s,
    const uint8_t* data, 
    size_t data_len, 
    sample_t* symbols, 
    size_t symbols_len,
    size_t* symbols_used) {
  for (size_t i = 0; i < data_len && 4*i < symbols_len; i++) {
    //it would be better to gray-code these, but we don't for compatibility
    symbols[4*i + 0] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * (data[i] & 0x03);
    symbols[4*i + 1] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * ((data[i] & 0x0C) >> 2);
    symbols[4*i + 2] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * ((data[i] & 0x30) >> 4);
    symbols[4*i + 3] = -SAMPLE_MAX + (SAMPLE_MAX/3) * 2 * ((data[i] & 0xC0) >> 6);
  }
  
  if (symbols_used != NULL) {
      *symbols_used = 4*i;
  }
  return i;
}
  
