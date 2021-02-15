#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "arg.h"
#include "util.c"

#define COMMAND(cmd, src) !strcmp((src), (cmd))
#define COMMAND_ARG(cmd, src) !strcmpt((src" "), (cmd), ' ')
#define GOTOXY(x, y) printf(yxstr, (y), (x))
#define GOUNDERT() GOTOXY(1, 10)

#define SUPERPOWERED(checker) (((checker) >> 7) & 1)
#define COLOR(checker) (((checker) >> 6) & 1)
#define COL(checker) (((checker >> 3) & 7) + 1)
#define ROW(checker) (((checker) & 7) + 1)

static void cmdmv(int16_t (*checkers)[2][12], char *cmd);
static void cmdreturn(int16_t (*checkers)[2][12]);
static void drawchecker(int16_t checker);
static void dumpcheckers(int16_t (*checkers)[2][12]);
static int16_t *getcheckerbypos(int16_t (*checkers)[2][12], int16_t row, int16_t col);
static void go(int16_t col, int16_t row);
static int16_t makechecker(int16_t superpowered, int16_t color, int16_t col, int16_t row);
static void prepare(int16_t (*checkers)[2][12]);

static const char t[] =
"\033[1;97m   a  b  c  d  e  f  g  h\n"
"\033[1;97m1 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m2 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n"
"\033[1;97m3 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m4 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n"
"\033[1;97m5 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m6 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n"
"\033[1;97m7 \033[0;37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\n"
"\033[1;97m8 \033[0;33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\033[33m[ ]\033[37m[ ]\n";

static const char yxstr[] = "\033[%d;%dH";
static int16_t color;

static void
cmdmv(int16_t (*checkers)[2][12], char *cmd)
{
	int16_t *checker, *dest;

	if (!strcmp(cmd, "help")) {
		usage:
		puts("usage: mv <src position> <dest position>");
		puts("   or: mv <src position> <movement>");
		puts("\nexample: mv c3 d4");
		puts(  "         mv c3 rd");
		puts("\n<movement> is 2 letters: [l]eft/[r]ight and [u]p/[d]own");
		return;
	}

	if (cmd[3] == 'l') cmd[3] = (cmd[0] - 1);
	else if (cmd[3] == 'r') cmd[3] = (cmd[0] + 1);

	if (cmd[4] == 'u' || cmd[4] == 't') cmd[4] = (cmd[1] - 1);
	else if (cmd[4] == 'd' || cmd[4] == 'b') cmd[4] = (cmd[1] + 1);

	if (
	!(   (cmd[0] >= 'a' && cmd[0] <= 'h')
	&&   (cmd[1] >= '1' && cmd[1] <= '8')
	&&   (cmd[2] == ' ')
	&&  ((cmd[3] >= 'a' && cmd[3] <= 'h') || (cmd[3] == 'l' || cmd[3] <= 'r'))
	&&  ((cmd[4] >= '1' && cmd[4] <= '8')
	||   (cmd[3] == 'u' || cmd[3] <= 'd' || cmd[3] == 't' || cmd[3] <= 'b'))
	)) goto usage;

	if ((checker = getcheckerbypos(checkers, cmd[0] - 'a' + 1, cmd[1] - '1' + 1)) == NULL) {
		puts("specified field is blank");
		return;
	}

	if (COLOR(*checker) != color) {
		puts("it's not your checker");
		return;
	}

	if ((dest = getcheckerbypos(checkers, cmd[3] - 'a' + 1, cmd[4] - '1' + 1)) != NULL) {
		if (COLOR(*dest) == color) {
			puts("you can't beat checker in your color");
			return;
		} else {
			*dest = -1;
			puts("*beating*");
		}
	}

	*checker = makechecker(
		SUPERPOWERED(*checker),
		COLOR(*checker),
		cmd[4] - '1',
		cmd[3] - 'a'
	);
}

static void
cmdreturn(int16_t (*checkers)[2][12])
{
}

static void
drawchecker(int16_t checker)
{
	if (checker < 0) return;
	go(COL(checker), ROW(checker));
	printf("\033[1;3%cm%c", COLOR(checker) + '1', SUPERPOWERED(checker) ? '@' : 'O');
}

static void
dumpcheckers(int16_t (*checkers)[2][12])
{
	int i, j;
	for (i = 0; i < 2; ++i)
		for (j = 0; j < 12; ++j) {
			drawchecker((*checkers)[i][j]);
		}
}

static int16_t *
getcheckerbypos(int16_t (*checkers)[2][12], int16_t row, int16_t col)
{
	int i, j;
	for (i = 0; i < 2; ++i)
		for (j = 0; j < 12; ++j)
			if (ROW((*checkers)[i][j]) == row && COL((*checkers)[i][j]) == col)
				return &((*checkers)[i][j]);
	return NULL;
}

static void
go(int16_t col, int16_t row)
{
	GOTOXY((3 * row) + 1, col + 1);
}

static int16_t
makechecker(int16_t superpowered, int16_t color, int16_t col, int16_t row)
{
	int16_t ret;
	ret = 0;
	ret += row & 7;
	ret += (col & 7) << 3;
	ret += (color & 1) << 6;
	ret += (superpowered & 1) << 7;
	return ret;
}

static void
prepare(int16_t (*checkers)[2][12])
{
	int i, j;
	for (i = 0; i < 2; ++i)
		for (j = 0; j < 12; ++j) {
			(*checkers)[i][j] = 0;
			(*checkers)[i][j] += (i << 6);
			(*checkers)[i][j] += (((j / 4) + (i * 5)) << 3);
			(*checkers)[i][j] += (((j % 4) * 2) + (!(((j / 4) + i) % 2)));
		}
}

int
main(int argc, char *argv[])
{
	char *line = NULL; size_t lnsiz = 0;

	/* 2 arrays of 12 checkers
	   in format:
	   [1 bit: superpowered][1 bit: color][3 bits: column][3 bits: row] */
	int16_t checkers[2][12];

	color = 1;
	prepare(&checkers);

	printf("\033[2J\033[H");

	while (1) {
		GOTOXY(1, 1);
		printf("%s", t);
		dumpcheckers(&checkers);
		GOUNDERT();
		printf("\033[0;97mstatus: %s\033[0;97m\n", "\033[1;92myour move");
		printf("\033[0;97m$ ");
		if (getline(&line, &lnsiz, stdin) < 0)
			break;
		line[strlen(line) - 1] = '\0';
		printf("\033[2J\033[H");
		GOTOXY(1, 12);

		if (COMMAND_ARG(line, "mv"))
			cmdmv(&checkers, line + 3);
		else if (COMMAND(line, "return"))
			cmdreturn(&checkers);
		else if (COMMAND_ARG(line, "rm"))
			printf("removed '%s'\n", line + 3);
		else if (COMMAND(line, "quit") || COMMAND(line, "exit") || COMMAND(line, "bye"))
			break;
		else if (COMMAND(line, ""));
		else
			printf("%s: unknown command or bad syntax\n", line);
	}
	printf("\033[2J\033[H");
	puts("goodbye!");
}
