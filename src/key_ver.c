/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*** data ***/
struct termios orig_termios;

/*** terminal ***/
void die(const char* s) {
  term_ClearScreen();
  perror(s);
  exit(1);
}

void term_DisableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("Disable raw mode, tcsetattr");
}

void term_EnableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("Enable raw mode, tcgetattr");

  atexit(term_DisableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |  ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("Enable raw mode, tcsetattr");
}

char term_ReadKey() {
  int nread;
  char c = '\0';
  while ( (nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("term_ReadKey, read");
  }
  return c;
}

void term_ClearScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H",3);
}

void term_SendCommand(const char* command) {
  write(STDOUT_FILENO, command, strlen(command));
}

/*** init ***/
int main() {
  term_EnableRawMode();
  
  while (1) {
    char c = '\0';
    read(STDIN_FILENO, &c, 1);
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q') break;
  }
  
  return 0;
}
