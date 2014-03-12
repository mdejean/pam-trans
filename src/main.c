#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "upconvert.h"
#include "convolve.h"
#include "encode.h"
#include "sample.h"

void testdump(const char* filename, const void* data, size_t size) {
  FILE* f = fopen(filename, "wb");
  fwrite(data, size, 1, f);
  fclose(f);
}

int main(int argc, char** argv) {
  encode_state encoder;
  convolve_state convolver;
  upconvert_state upconverter;

  FILE* test_file = NULL;

  const char* header = "IntroDigitalCommunications:eecs354:mxb11:profbuchner";

  const char* test_message =
"It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness, "
"it was the epoch of belief, it was the epoch of incredulity, it was the season of Light, it was the season of "
"Darkness, it was the spring of hope, it was the winter of despair, we had everything before us, we had "
"nothing before us, we were all going direct to Heaven, we were all going direct the other way—in short, "
"the period was so far like the present period, that some of its noisiest authorities insisted on its being "
"received, for good or for evil, in the superlative degree of comparison only.";
  size_t test_message_len = strlen(test_message);

  test_file = fopen("test.out", "wb");
  if (!test_file) return 1;

  encode_init(&encoder, 100, (const uint8_t*)header, strlen(header), NULL, 0);
  convolve_init_srrc(&convolver, 0.2, 4, 100); //baudrate = 10kHz
  upconvert_init(&upconverter, 1, 10); //carrier = 100kHz if Fs = 1MHz


  //TODO: should be able to ask for this value, though we're not going to be dynamically allocating it on the micro
  size_t data_len = test_message_len + (test_message_len/100 + 1) * strlen(header);
  uint8_t* data = malloc(data_len);
  size_t symbols_len = 4*data_len;
  sample_t* symbols = malloc(symbols_len * sizeof(sample_t));

  sample_t envelope[2048];
  sample_t signal[2048];

  size_t data_used = frame_message(&encoder, (const uint8_t*)test_message, test_message_len, data, data_len);

  size_t symbols_used = encode_data(&encoder, data, data_used, symbols, symbols_len);

  testdump("srrc.out",
           convolver.pulse_shape,
           convolver.pulse_shape_len * sizeof(sample_t));

  for (size_t i = 0; i < symbols_used - 4; ) { //last width symbols can only be written with some bogus data
    memset(state->edge_symbols, 0, overlap * sizeof(sample_t));
    size_t symbols_convolved = convolve(&convolver,
      &symbols[i], (symbols_used - i < 20) ? symbols_used - i : 20,
      envelope, 2048);
    i += symbols_convolved;
    //really need to be asking convolve for how much signal it used rather than *100 here
    size_t signal_used = upconvert(&upconverter, envelope, symbols_convolved * 100, signal, 2048);
    printf("turned %u symbols into %u samples (%u/%u)\n",
      symbols_convolved, signal_used, i, symbols_used);
    fwrite(signal, sizeof(signal[0]), signal_used, test_file);
  }

  fclose(test_file);
  free(data);
  free(symbols);
  return 0;
}
