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
// taken from https://www.geeksforgeeks.org/how-to-convert-an-integer-to-a-string-in-c/
void intToStr(int num, char* str)
{
    int i = 0;
    // Save the sign of the number
    int sign = num;

    // If the number is negative, make it positive
    if (num < 0) num *= -1;

    // Extract digits from the number and add them to the
    // string
    do {
        // Convert integer digit to character
        str[i++] = num % 10 + '0';
    } while ((num /= 10) > 0);

    // If the number was negative, add a minus sign to the
    // string
    if (sign < 0) {
        str[i++] = '-';
    }

    // Null-terminate the string
    str[i] = '\0';

    // Reverse the string to get the correct order
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

int addNumberToString(char** outStr, const char* ogStr, int argc, ...) {
  va_list args;
  va_start(args,argc);
  
  char nStrArr[argc][12];
  
  // outStr length is original string length - number of substitution markers (which should be equal to argc)
  // + the length of every number converted to string. 
  int outLen = strlen(ogStr) - argc;
  
  // calculating the total length of outStr, and putting args into a proper array.
  for (int i = 0; i < argc; ++i) {
    intToStr(va_arg(args,int),nStrArr[i]);
    outLen += strlen(nStrArr[i]);
  }

  *outStr =  (char*) malloc((outLen+1)*sizeof(char));

  int outIndex = 0;
  int nArrIndex = 0;
  for (int ogIndex = 0; ogIndex < (int) strlen(ogStr) ; ++ogIndex) {
    if (ogStr[ogIndex] == HARI_SUB_MARKER) {
      int len = strlen(nStrArr[nArrIndex]);
      memcpy((*outStr)+outIndex, nStrArr[nArrIndex], len);
      outIndex += len;
      ++nArrIndex;
      continue;
    }
    (*outStr)[outIndex++] = ogStr[ogIndex];
  }

  (*outStr)[outLen] = '\n';

  va_end(args);
  return HARI_SUCCESS;
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
  const char* SET_CURSOR = "\x1b[%;%H";
  char* command;
  if (addNumberToString(&command, SET_CURSOR, 2, v, h) != HARI_SUCCESS) {
    // error treatment
  }
  // printf("\r\nCommand: %s\r\n",command);
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
  cursor.v0 = 2;
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
int editor_ProcRegularEscSeq(char** command) {
  int commandLen = HARI_INIT_COMM_LEN;
  char treated = '\0';
  while (treated < 'a' && treated > 'z') {
    char c = term_ReadKey();
    int currentLen = strlen(*command);
    if (currentLen + 1 == commandLen) {
      commandLen *= 2;
      *command = realloc(*command,commandLen*sizeof(char));
    }
    (*command)[currentLen] = c;
    treated = c | 0x20;
  }
  return HARI_SUCCESS;
}

int editor_ProcessEscapeSequence(char c) {
  char* command = (char*) malloc(HARI_INIT_COMM_LEN*sizeof(char));
  int code = HARI_SUCCESS;
  command[0] = c;
  command[1] = term_ReadKey();
  switch (command[1]) {
    case '[':
      code = editor_ProcRegularEscSeq(&command);
      break;
    default:
      code = HARI_ERR_COMM_SEQ;
      break;
  }
  term_SendCommand(command);
  return code;
}

void editor_ProcessKeyPress() {
  char c = term_ReadKey();

  switch (c) {
    case '\x1b':
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
