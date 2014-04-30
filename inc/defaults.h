#ifndef DEFAULTS_H
#define DEFAULTS_H

#define DEFAULT_OUTPUT_PERIOD 200

#define DEFAULT_MESSAGE \
"It was the best of times, it was the worst of times, it was the age of wisdom, it was the age of foolishness, " \
"it was the epoch of belief, it was the epoch of incredulity, it was the season of Light, it was the season of " \
"Darkness, it was the spring of hope, it was the winter of despair, we had everything before us, we had " \
"nothing before us, we were all going direct to Heaven, we were all going direct the other way-in short, " \
"the period was so far like the present period, that some of its noisiest authorities insisted on its being " \
"received, for good or for evil, in the superlative degree of comparison only."

#define DEFAULT_MESSAGE_B \
"This is a short message to test the Pulse Amplitude Modulation Digital Radio Transmitter"

#define DEFAULT_MESSAGE_C \
"Messages are broken up into frames of (by default) 100 characters. Though all of these messages are text, "\
"the PAM transmitter is capable of transmitting any binary data."

#define DEFAULT_HEADER "IntroDigitalCommunications:eecs354:mxb11:profbuchner"
#define DEFAULT_FRAME_LENGTH 100

#define DEFAULT_CONVOLVE_OVERSAMPLING 100
#define DEFAULT_OVERLAP_SYMBOLS 4
#define DEFAULT_BETA 0.2f

#define DEFAULT_UPCONVERT_PERIOD 10
#define DEFAULT_UPCONVERT_OVERSAMPLING 1

#endif
