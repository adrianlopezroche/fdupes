#include <string.h>
#include "sigint.h"

volatile sig_atomic_t got_sigint = 0;

void sigint_handler(int signal)
{
  got_sigint = 1;
}

void register_sigint_handler()
{
  struct sigaction action;

  memset(&action, 0, sizeof(struct sigaction));

  action.sa_handler = sigint_handler;
  sigaction(SIGINT, &action, 0);	
}