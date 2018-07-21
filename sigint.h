#ifndef SIGINT_H
#define SIGINT_H

#include <signal.h>

extern volatile sig_atomic_t got_sigint;

void register_sigint_handler();

#endif