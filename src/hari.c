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

#define HARI_INIT_COMM_LEN 16
#define HARI_ERR_COMM_SEQ 1
#define HARI_SUCCESS 0
void editorClearScreen();
void die(const char* s);

/*** data ***/
struct termios orig_termios;

const char* HARI_ERR_TABLE[] = {
  "Everything normal",
  "Unrecognized escape command",
};

const int HARI_ERR_TABLE_LEN = sizeof(HARI_ERR_TABLE) / sizeof(const char*);

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

void sendCommandToTerminal(const char* command) {
  write(STDOUT_FILENO, command, strlen(command));
}

void editorMoveCursor(int up, int right) {
  const char* MOVE_UP = "\x1b[%dA";
  const char* MOVE_DOWN = "\x1b[%dB";
  const char* MOVE_RIGHT = "\x1b[%dC";
  const char* MOVE_LEFT = "\x1b[%dD";

  char* vert;
  if (up > 0) {
    addNumberToString(&vert, MOVE_UP, up);
    sendCommandToTerminal(vert);
  }
  if (up < 0) {
    addNumberToString(&vert, MOVE_DOWN, -up);
    sendCommandToTerminal(vert);
  }
  free(vert);

  char* hori;
  if (right > 0) {
    addNumberToString(&hori, MOVE_RIGHT, right);
    sendCommandToTerminal(hori);
  }
  if (right < 0) {
    addNumberToString(&hori, MOVE_LEFT, right);
    sendCommandToTerminal(hori);
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
int editorProcRegularEscSeq(char** command) {
  int commandLen = HARI_INIT_COMM_LEN;
  char treated = '\0';
  while (treated < 'a' && treated > 'z') {
    char c = editorReadKey();
    int currentLen = strlen(*command);
    if (currentLen + 1 == commandLen) {
      commandLen *= 2;
      *command = realloc(*command,commandLen*sizeof(char));
    }
    (*command)[currentLen] = c;
    treated = c | 0x20;
  }
  return 0;
}

int editorProcessEscapeSequence(char c) {
  char* command = (char*) malloc(HARI_INIT_COMM_LEN*sizeof(char));
  int code = HARI_SUCCESS;
  command[0] = c;
  command[1] = editorReadKey();
  switch (command[1]) {
    case '[':
      code = editorProcRegularEscSeq(&command);
      break;
    default:
      code = HARI_ERR_COMM_SEQ;
      break;
  }
  sendCommandToTerminal(command);
  return code;
}

void editorProcessKeyPress() {
  char c = editorReadKey();

  switch (c) {
    case '\x1b':
      break;
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
