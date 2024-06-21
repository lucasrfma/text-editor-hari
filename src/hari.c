/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f)

#define HARI_INIT_COMM_LEN 16
#define HARI_SUB_MARKER '%'

// error codes
#define HARI_SUCCESS 0
#define HARI_ERR_COMM_SEQ 1

const char* HARI_ERR_TABLE[] = {
  "Everything normal",
  "Unrecognized escape command",
};
const int HARI_ERR_TABLE_LEN = sizeof(HARI_ERR_TABLE) / sizeof(const char*);

void term_ClearScreen();
void die(const char* s);

/*** data ***/
struct termios orig_termios;

// Cursor Data
struct Cursor {
  int v0;
  int vDelta;
  int h0;
  int hDelta;
};

struct Cursor cursor = {
  .v0 = 3,
  .vDelta = 0,
  .h0 = 5,
  .hDelta = 0
};

/*** util ***/
void vsnprintfWrapper(char** outStr, const char* format, ...)
{
  int buffer = 48;
  *outStr = (char*) malloc(buffer*sizeof(char));
 
  va_list args;
  va_start(args, format);

  int realSize = vsnprintf(*outStr, buffer, format, args);
  va_end(args);

  if (realSize >= buffer) {
    buffer = realSize+1;
    *outStr = (char*) realloc(*outStr, buffer*sizeof(char));
    va_start(args, format);
    vsnprintf(*outStr, buffer, format, args);
    va_end(args);
  }
}

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

void term_SetCursor() {
  int v = cursor.v0 + cursor.vDelta;
  int h = cursor.h0 + cursor.hDelta;
  const char* SET_CURSOR = "\x1b[%d;%dH";
  char* command;
  vsnprintfWrapper(&command, SET_CURSOR, v, h);
  term_SendCommand(command);
}

/*** output ***/
void editor_PrintMenu() {
  printf("[Ctrl+Q] Quit");
  printf("\r\n______________________________________________\r\n");
}

void editor_MoveCursor(int down, int right) {
  int hDelta = cursor.hDelta + right;
  int vDelta = cursor.vDelta + down;
  cursor.hDelta = hDelta >= 0 ? hDelta : 0;
  cursor.vDelta = vDelta >= 0 ? vDelta : 0;
}

void editor_PrintBlankLines() {
  const int HEIGHT = 5;
  const int WIDTH = 4;
  cursor.h0 = WIDTH + 1;
  cursor.v0 = 3;
  for (int y = 0; y < HEIGHT; ++y) {
    for (int x = 1; x < WIDTH; ++x) {
      printf("-");
    }
    printf("|\r\n");
  }
}

void editor_RefreshScreen() {
  term_ClearScreen();
  editor_PrintMenu();
  editor_PrintBlankLines();
  term_SetCursor();
}

/*** input ***/
int editor_ProcRegularEscSeq() {
  int code = HARI_SUCCESS;
  char c = term_ReadKey();
  switch (c) {
    case 'A':
      editor_MoveCursor(-1,0);
      break;
    case 'B':
      editor_MoveCursor(1,0);
      break;
    case 'C':
      editor_MoveCursor(0,1);
      break;
    case 'D':
      editor_MoveCursor(0,-1);
      break;
    default:
      code = HARI_ERR_COMM_SEQ;
      break;
  }
  return code;
}

int editor_ProcessEscapeSequence() {
  int code = HARI_SUCCESS;
  char c = term_ReadKey();
  switch (c) {
    case '[':
      code = editor_ProcRegularEscSeq();
      break;
    default:
      code = HARI_ERR_COMM_SEQ;
      break;
  }
  return code;
}

void editor_ProcessKeyPress() {
  char c = term_ReadKey();

  if(isprint(c) != 0)
  {
    
  }

  switch (c) {
    case '\x1b':
      editor_ProcessEscapeSequence(c);
      break;
    case CTRL_KEY('q'):
      term_ClearScreen();
      exit(0);
      break;
  }
}

/*** init ***/
int main() {
  term_EnableRawMode();
  // editor_RefreshScreen();
  // editor_ProcessKeyPress();
  while (1) {
   editor_RefreshScreen();
   editor_ProcessKeyPress();
  }

  return 0;
}
