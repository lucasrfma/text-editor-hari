/*** includes ***/
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
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
const int HEADER_HEIGHT = 2;

struct editorConfig {
  struct termios orig_termios;
  int screenRows;
  int screenCols;
  int firstPrintedRow;
};

struct editorConfig E = {
  .firstPrintedRow = 1
};

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
int util_NumberOfDigits(int n) {
  int i = 1;
  while ((n = n / 10) > 0) ++i;
  return i;
}

void util_vsnprintfWrapper(char** outStr, const char* format, ...)
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

/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}
void abFree(struct abuf *ab) {
  free(ab->b);
}

/*** terminal ***/
void die(const char* s) {
  term_ClearScreen();
  perror(s);
  exit(1);
}

void term_DisableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("Disable raw mode, tcsetattr");
}

void term_EnableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("Enable raw mode, tcgetattr");

  atexit(term_DisableRawMode);

  struct termios raw = E.orig_termios;
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
  // write(STDOUT_FILENO, "\x1b[H",3);
}

void term_SendCommand(const char* command) {
  write(STDOUT_FILENO, command, strlen(command));
}

void term_SetCursor() {
  int v = cursor.v0 + cursor.vDelta;
  int h = cursor.h0 + cursor.hDelta;
  const char* SET_CURSOR = "\x1b[%d;%dH";
  char* command;
  util_vsnprintfWrapper(&command, SET_CURSOR, v, h);
  term_SendCommand(command);
}

void term_GetCursorPosition(int *rows, int *cols) {
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) die("Couldn't read cursor position.");

  char buf[32];
  unsigned int i = 0;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    ++i;
  }
  buf[i] = '\0';
  
  printf("\r\n&buf[1]: '%s'\r\n", &buf[2]);
  if (buf[0] != '\x1b' || buf[1] != '[') die("GetCursorPosition: not the expected escape seq");
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) die("GetCursorPosition: not the expected values");
}

void term_UpdateWindowSize() {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) die("Couldn't position cursor at end");
    term_GetCursorPosition(&E.screenRows, &E.screenCols);
    return;
  }
  E.screenCols = ws.ws_col;
  E.screenRows = ws.ws_row;
}

/*** output ***/
void editor_PrintMenu() {
  printf("[Ctrl+Q] Quit");
  printf("\r\n______________________________________________\r\n");
}

void editor_FirstPosition(int* v, int* h) {
  *v = HEADER_HEIGHT + 1;
  *h = util_NumberOfDigits(E.firstPrintedRow + E.screenRows - HEADER_HEIGHT) + 1;
}

void editor_MoveCursor(int down, int right) {
  int hDelta = cursor.hDelta + right;
  int vDelta = cursor.vDelta + down;
  cursor.hDelta = hDelta >= 0 ? hDelta : 0;
  cursor.vDelta = vDelta >= 0 ? vDelta : 0;
}

void editor_PrintBlankLines() {
  cursor.v0 = HEADER_HEIGHT + 1;
  const int height = E.screenRows - cursor.v0;
  const int width = util_NumberOfDigits(E.firstPrintedRow + E.screenRows - HEADER_HEIGHT);
  cursor.h0 = width + 1;
  for (int y = 0; y < height; ++y) {
    for (int x = 1; x < width; ++x) {
      write(STDOUT_FILENO,"-",1);
    }
    write(STDOUT_FILENO, "|\r\n", 3);
  }
}

void editor_RefreshScreen() {
  term_UpdateWindowSize();
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
