/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)
void editorClearScreen();
void die(const char* s);

/*** data ***/
struct termios orig_termios;

/*** util ***/
int countDigits(int n) {
  int count = 1;
  while ((n = n/10) > 0) ++count;
  return count;
}

/**
  In this function we take an ogStr (original string) which we expect to have a %d and no other substitution markers
  we also receive a number to substitute in.
  outStr will be the receptacle of this result. This function allocates the necessary memory.
**/
void addNumberToString(char** outStr, const char* ogStr, int n) {
  // subtract 2 because we are not only adding the string representation of the number
  // but substituting the "%d" in the original string.
  int outLen = countDigits(n) + strlen(ogStr) - 2;
  // add 1 to the allocation calc bc C strings must end with '\0'
  *outStr =  (char*) malloc((outLen+1)*sizeof(char));
  sprintf(*outStr, ogStr, n);
}

/*** terminal ***/
void die(const char* s) {
  editorClearScreen();
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("Disable raw mode, tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("Enable raw mode, tcgetattr");

  atexit(disableRawMode);

  struct termios raw = orig_termios;
  raw.c_iflag &= ~(ICRNL | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |  ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("Enable raw mode, tcsetattr");
}

char editorReadKey() {
  int nread;
  char c = '\0';
  while ( (nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("editorReadKey, read");
  }
  return c;
}

void editorClearScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H",3);
}

void editorMoveCursor(int up, int right) {
  const char* MOVE_UP = "\x1b[%dA";
  const char* MOVE_DOWN = "\x1b[%dB";
  const char* MOVE_RIGHT = "\x1b[%dC";
  const char* MOVE_LEFT = "\x1b[%dD";

  char* vert;
  if (up > 0) {
    addNumberToString(&vert, MOVE_UP, up);
    write(STDOUT_FILENO, vert, strlen(vert));
  }
  if (up < 0) {
    addNumberToString(&vert, MOVE_DOWN, -up);
    write(STDOUT_FILENO, vert, strlen(vert));
  }
  free(vert);

  char* hori;
  if (right > 0) {
    addNumberToString(&hori, MOVE_RIGHT, right);
    write(STDOUT_FILENO, hori, strlen(hori));
  }
  if (right < 0) {
    addNumberToString(&hori, MOVE_LEFT, right);
    write(STDOUT_FILENO, hori, strlen(hori));
  }
  free(hori);
}

/*** output ***/
void editorPrintMenu() {
  printf("[Ctrl+Q] Quit");
  printf("\r\n______________________________________________\r\n");
}

void editorPrintBlankLines() {
  const int HEIGHT = 5;
  const int WIDTH = 4;
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 1; x < WIDTH; ++x) {
      printf("-");
    }
    printf("|\r\n");
  }
  editorMoveCursor(HEIGHT,WIDTH);
}

void editorRefreshScreen() {
  editorClearScreen();
  editorPrintMenu();
  editorPrintBlankLines();
}

/*** input ***/
void editorProcessKeyPress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'):
      editorClearScreen();
      exit(0);
      break;
  }
}

/*** init ***/
int main() {
  enableRawMode();
  while (1) {
   editorRefreshScreen();
   editorProcessKeyPress();
  }

  return 0;
}
