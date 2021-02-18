/* See AUTHORS file for copying details
   and LICENSE file for license details. */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "arg.h"
#include "util.c"

#define COMMAND(cmd, src) !strcmp((src), (cmd))
#define COMMAND_ARG(cmd, src) !strcmpt((src" "), (cmd), ' ')
#define GOTOXY(x, y) printf(yxstr, (y), (x))
#define GOUNDERT() GOTOXY(1, 10)
#define RETONFAIL(statement) if ((statement) < -1) return -1

#define FIGURE(card) (((card) >> 4) & 3)
#define VALUE(card) (((card) & 15))

static int cardcount(int8_t *stack);
static void cmddraw(int8_t *dest, int8_t *src, int count);
static void cmdmv(int8_t *stack, int8_t index);
static void cmdreturn(struct sockaddr_in addr, int *move, int color);
static void drawcard(int8_t card);
static void dumpcards(int8_t *stack);
static int8_t *getcardbyindex(int8_t *stack, int8_t index);
static int movecard(int8_t *deststack, int8_t *srcstack, int8_t index);
static void prepare(int8_t *stack);
static void shuffle(int8_t *array, size_t n);
static void sort(int8_t *array, size_t n, char *cmd);
static void usage(void);

static size_t message(struct sockaddr_in addr, char *msg, size_t msgsiz, char **buf, size_t bufsiz);
static void sendupdate(int8_t *stack, struct sockaddr_in addr);
static void requestjoin(struct sockaddr_in addr, struct sockaddr_in haddr);

char *argv0;

static const char *symbols[] = { "♠", "♥", "♣", "♦" };
static const char values[] = "234567890JKQA?";
static const char yxstr[] = "\033[%d;%dH";
static int8_t color;

static int
cardcount(int8_t *stack)
{
	int i, ret;
	for (i = ret = 0; i < 52; ++i)
		if (stack[i] >= 0) ++ret;
	return ret;
}

static void
cmddraw(int8_t *dest, int8_t *src, int count)
{
	int i;
	for (i = 0; i < count; ++i)
		switch (movecard(dest, src, 0)) {
		case 1: puts("stack is empty");     return; break;
		case 2: puts("no space available"); return; break;
		}
}

static void
cmdmv(int8_t *stack, int8_t index)
{
	switch (movecard(stack + 52, stack + (52 * (color + 2)), index)) {
	case 1: puts("specified field is blank");   return; break;
	case 2: puts("no space available");         return; break;
	}
}

static void
cmdreturn(struct sockaddr_in addr, int *move, int color)
{
	*move = !color;
	char msg = 'R';
	if (message(addr, (char *)&msg, 1, NULL, 0) < 0)
		die("message:");
}

static void
cmdrestack(int8_t *stack)
{
	int i;
	for (i = 0; i < cardcount(stack + 52); ++i)
		switch (movecard(stack, stack + 52, 0)) {
		case 1: puts("specified field is blank");   return; break;
		case 2: puts("no space available");         return; break;
		}
}

static void
drawcard(int8_t card)
{
	if (card < 0) return;
	printf("\033[1;37m[\033[1;%sm%s %c%c\033[1;37m]",
			(FIGURE(card) % 2) ? "31" : "97", symbols[FIGURE(card)],
			VALUE(card) == 8 /* 10 */ ? '1' : ' ',
			values[VALUE(card)]);
}

static void
dumpcards(int8_t *stack)
{
	int i, j, c;
	printf("\033[1;37m[\033[1;96m@ \033[1;97m%2d\033[1;37m]",
			cardcount(stack));
	for (i = j = 0; i < 52; ++i) {
		if (stack[i + 52] >= 0)
			if (!(++j % 12))
				puts("");
		drawcard(stack[i + 52]);
	}
	puts("\n————————————————————————————————————————————————————————————————");
	for (i = 0, c = j = -1; i < 52; ++i) {
		if (stack[i + 52 + (52 * (color + 1))] >= 0)
			if (!(++j % 12)) {
				int k, l;
				if (!(c % 12)) puts("");
				for (k = 0, l = 0; k < 52; ++k) {
					if (stack[k + 52 + (52 * (color + 1))] >= 0)
						printf("\033[1;33m│\033[1;37m#\033[1;97m%03d\033[1;33m│", ++c);
					if (!(++l % 12)) break;
				}
				puts("");
			}
		drawcard(stack[i + 52 + (52 * (color + 1))]);
	}
}

static int8_t *
getblankcard(int8_t *stack)
{
	int i;
	for (i = 0; i < 52; ++i)
		if (stack[i] < 0)
			return stack + i;
	return NULL;
}

static int8_t *
getcardbyindex(int8_t *stack, int8_t index)
{
	int i, j;
	if (index >= 0) {
		for (i = 0, j = -1; i < 52; ++i)
			if (stack[i] != -1 && ++j == index)
				return stack + i;
	} else {
		for (i = 51; i > -1; --i)
			if (stack[i] != -1)
				return stack + i;
	}
	return NULL;
}

static int
movecard(int8_t *deststack, int8_t *srcstack, int8_t index)
{
	int8_t *src, *dest;

	if ((src = getcardbyindex(srcstack, index)) == NULL);
		/* return 1; */

	if ((dest = getblankcard(deststack)) == NULL)
		return 2;

	*dest = *src;
	*src = -1;

	return 0;
}

static void
prepare(int8_t *stack)
{
	int i, j;
	for (i = 0; i < 52; ++i)
		stack[i] = 0 + (i % 13) + ((i / 13) << 4);
	shuffle(stack, 52);
	for (j = 1; j < 4; ++j)
		for (i = 0; i < 52; ++i)
			stack[i + (52 * j)] = -1;
}

static void
shuffle(int8_t *array, size_t n)
{
	size_t i, j;
	int t;
    if (n > 1) {
        for (i = 0; i < n - 1; i++) {
          j = i + rand() / (RAND_MAX / (n - i) + 1);
          t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}

static void
sort(int8_t *array, size_t n, char *cmd)
{
	int i, j, k;
	if (COMMAND(cmd, "f") || COMMAND(cmd, "fig") || COMMAND(cmd, "figure")) {
		for (i = 0; i < n; ++i)
			for (j = 0; j < n - i - 1; ++j)
				if (array[j] > array[j + 1]) {
					k = array[j];
					array[j] = array[j + 1];
					array[j + 1] = k;
				}
	} else {
		for (i = 0; i < n; ++i)
			for (j = 0; j < n - i - 1; ++j)
				if (((array[j] & (1 << 8)) - 1) > ((array[j + 1] & (1 << 8)) - 1)) {
					k = array[j];
					array[j] = array[j + 1];
					array[j + 1] = k;
				}
	}
}

static void
usage(void)
{
	die("usage: %s [-h HOSTIP] [-p HOSTPORT] [CLIENT_IP [CLIENT_PORT]]", argv0);
}

static size_t
message(struct sockaddr_in addr, char *msg, size_t msgsiz, char **buf, size_t bufsiz)
{
	int sockfd;
	size_t rb = 0;

	RETONFAIL(sockfd = socket(addr.sin_family, SOCK_STREAM, IPPROTO_TCP));
	RETONFAIL(connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)));
	RETONFAIL(send(sockfd, msg, msgsiz, 0));

	if (buf != NULL)
		RETONFAIL(rb = read(sockfd, *buf, bufsiz));

	close(sockfd);
	return rb;
}

static void
sendupdate(int8_t *stack, struct sockaddr_in addr)
{
	char msg[1 + (52 * 4)];
	msg[0] = 'U';
	memcpy(msg + 1, stack, (52 * 4));
	if (message(addr, msg, 1 + (52 * 4), NULL, 0) < 0)
		die("message:");
}

static void
requestjoin(struct sockaddr_in addr, struct sockaddr_in haddr)
{
	char msg[1 + sizeof(addr)] = "J";
	memcpy(msg + 1, &haddr, sizeof(haddr));
	if (message(addr, (char *)&msg, 1 + sizeof(addr), NULL, 0) < 0)
		die("message:");
}

int
main(int argc, char *argv[])
{
	char *line = NULL; size_t lnsiz = 0;
	pid_t forkpid, parentpid;
	sigset_t sig; int signo;
	socklen_t caddrsiz;
	char *ad; int *move;
	int8_t *stacks;

	struct sockaddr_in haddr, caddr;
	struct sockaddr_in *chost = mmap(NULL, sizeof(*chost), PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_SHARED, 0, 0);

	ad = mmap(NULL, sizeof(*ad), PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_SHARED, 0, 0);

	move = mmap(NULL, sizeof(*move), PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_SHARED, 0, 0);

	/* array divided to 4 parts (* 52):
	   - drawing stack
	   - putting stack
	   - A player's stack
	   - B player's stack
	   format:
	   [1 bit: sign, used to recognize blank field for a card]
	   [1 bits: reserved for later usage][2 bits: figure]
	   [4 bits: value (2-10, J/Q/K/A, Joker (?), 2*nothing)] */
	stacks = mmap(NULL, sizeof(*stacks) * 52 * 4, PROT_READ | PROT_WRITE,
			MAP_ANONYMOUS | MAP_SHARED, 0, 0);

	haddr.sin_family = caddr.sin_family = chost->sin_family = AF_INET;
	haddr.sin_port = caddr.sin_port = chost->sin_port = htons(4361);
	haddr.sin_addr.s_addr = caddr.sin_addr.s_addr =
		chost->sin_addr.s_addr = inet_addr("127.0.0.1");

	caddrsiz = sizeof(caddr);

	ARGBEGIN {
	case 'h': haddr.sin_addr.s_addr = inet_addr(ARGF());				break;
	case 'p': haddr.sin_port        = htons(strtol(ARGF(), NULL, 10));	break;
	default: usage(); break;
	} ARGEND

	if (argc > 2)
		die("too many arguments (given: %d; required: from 0 to 2)", argc);

	*move = 0;

	if (!argc) /* argc not given, hosting */
		color = 0;
	else /* argc given, joining */
		color = 1;

	if (argc > 0)
		chost->sin_addr.s_addr = inet_addr(argv[0]);
	if (argc > 1)
		chost->sin_port = htons(strtol(argv[1], NULL, 10));

	prepare(stacks);
	srand(parentpid = getpid());

	/* fork */ {
		int sockfd, clientfd, opt; size_t resplen;
		char buffer[BUFSIZ];

		if ((sockfd = socket(haddr.sin_family, SOCK_STREAM, IPPROTO_TCP)) < 0)
			die("socket:");

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					&opt, sizeof(opt)) < 0)
			die("setsockopt:");

		if (bind(sockfd, (const struct sockaddr *)&haddr, sizeof(haddr)) < 0)
			die("bind:");
		if (listen(sockfd, 3) < 0)
			die("listen:");

		if ((forkpid = fork()) < 0)
			die("fork:");

		if (!forkpid) {
			while (1) {
				if ((clientfd = accept(sockfd, (struct sockaddr *)&caddr, &caddrsiz)) < 0)
					die("accept:");

				if ((resplen = read(clientfd, buffer, BUFSIZ)) < 0)
					die("read:");

				switch (*buffer) {
				/* fallthrough */
				case 'A': /* Accepted */
				case 'D': /* Denied */
					*ad = *buffer;
					kill(parentpid, SIGUSR1);
					break;
				case 'J': /* Join */ {
					char *l = NULL; size_t lsiz = 0;
					char AD[] = "AD";
					memcpy(chost, buffer + 1, resplen - 1);
					printf("accept connection request from %s:%d (hosts at %s:%d)? [y/n]: ",
							inet_ntoa(caddr.sin_addr), htons(caddr.sin_port),
							inet_ntoa(chost->sin_addr), htons(chost->sin_port));
					if (getline(&l, &lsiz, stdin) < 0)
						die("error while getting line:");
					if (*l == 'y' || *l == 'Y')
						kill(parentpid, SIGUSR1);
					message(*chost, (*l == 'y' || *l == 'Y') ? AD : AD + 1, 1, NULL, 0);
					free(l);
					break;
				}
				case 'R': /* Return */
					*move = color;
					kill(parentpid, SIGUSR1);
					break;
				case 'U': /* Update */
					memcpy(stacks, buffer + 1, resplen - 1);
					kill(parentpid, SIGUSR1);
					break;
				}

				close(clientfd);
			}
		}
	}

	sigemptyset(&sig);
	sigaddset(&sig, SIGUSR1);
	sigprocmask(SIG_BLOCK, &sig, NULL);

	if (!argc)
		printf("\033[2J\033[Hhosting under %s:%d\nwaiting for other users...\n",
				inet_ntoa(haddr.sin_addr), htons(haddr.sin_port));
	else {
		printf("\033[2J\033[Hwaiting for %s:%d to acceptation...\n",
				inet_ntoa(caddr.sin_addr), htons(caddr.sin_port));
		requestjoin(*chost, haddr);
	}

	sigwait(&sig, &signo);
	if (argc && *ad != 'A') {
		kill(forkpid, SIGTERM);
		die("access denied");
	}
	printf("\033[2J\033[H");

	while (1) {
		GOTOXY(1, 1);
		dumpcards(stacks);
		puts("");
		printf("\033[0;97mstatus: \033[1;9%cm%s\033[0;97m\n", *move + '1', *move == color ? "your move" : "waiting");
		printf("\033[0;97mcard count: \033[1;91m%d\033[1;37m:\033[1;92m%d\033[0;97m\n", cardcount(stacks + 52 * 2), cardcount(stacks + 52 * 3));

		if (*move != color)
			sigwait(&sig, &signo);
		else {
			printf("\033[0;97m$ ");

			if (getline(&line, &lnsiz, stdin) < 0)
				break;
			line[strlen(line) - 1] = '\0';
			printf("\033[2J");

			if (COMMAND(line, "draw")) {
				cmddraw(stacks + (52 * (color + 2)), stacks, 1);
				sendupdate(stacks, *chost);
			} else if (COMMAND_ARG(line, "draw")) {
				cmddraw(stacks + (52 * (color + 2)), stacks, strtol(line + 5, NULL, 10));
				sendupdate(stacks, *chost);
			} else if (COMMAND_ARG(line, "mv")) {
				cmdmv(stacks, strtol(line + 3, NULL, 10));
				sendupdate(stacks, *chost);
			} else if (COMMAND(line, "next")) {
				cmddraw(stacks + 52, stacks, 1);
				sendupdate(stacks, *chost);
			} else if (COMMAND(line, "restack")) {
				cmdrestack(stacks);
				sendupdate(stacks, *chost);
			} else if (COMMAND(line, "return") || COMMAND(line, "ret"))
				cmdreturn(*chost, move, color);
			else if (COMMAND(line, "shuffle")) {
				shuffle(stacks, 52);
				sendupdate(stacks, *chost);
			} else if (COMMAND(line, "sort")) {
				sort(stacks, 52, line);
				sendupdate(stacks, *chost);
			} else if (COMMAND(line, "take")) {
				movecard(stacks + (52 * (color + 2)), stacks + 52, -1);
				sendupdate(stacks, *chost);
			} else if (COMMAND(line, "quit") || COMMAND(line, "exit")
					|| COMMAND(line, "bye"))
				break;
			else if (COMMAND(line, ""));
			else
				printf("%s: unknown command or bad syntax\n", line);
		}
	}

	kill(forkpid, SIGTERM);
	munmap(stacks, sizeof(*stacks) * 52 * 4);
	munmap(chost, sizeof(*chost));
	munmap(move, sizeof(*move));
	munmap(ad, sizeof(*ad));
	printf("\033[2J\033[H");
	puts("goodbye!");
}
