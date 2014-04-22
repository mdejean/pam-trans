#ifndef ENCODE_H
#define ENCODE_H

#include <stdbool.h>
#include "sample.h"


typedef struct encode_state_ {
  size_t frame_len; //number of message *bytes* in each frame
  const uint8_t* start_framing;
  size_t start_framing_len;
  const uint8_t* end_framing;
  size_t end_framing_len;
} encode_state;

bool encode_init(encode_state* s);

/* frame_message - add framing bits to start of message
returns: amount of data used
*/
size_t frame_message(
    encode_state* s, 
    const uint8_t* message, 
    size_t message_len, 
    uint8_t* data, 
    size_t data_len,
    size_t* data_used);

/* encode_data - turn framed message into PAM symbols 
returns: amount of symbols used
letters2pam*/
size_t encode_data(
    encode_state* s, 
    const uint8_t* data, 
    size_t data_len, 
    sample_t* symbols, 
    size_t symbols_len,
    size_t* symbols_used);

#endif
