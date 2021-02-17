PREFIX = /usr/local
CC = gcc

CFLAGS = -std=c99 -pedantic -D_DEFAULT_SOURCE

nmak: nmak.c
	${CC} ${CFLAGS} ${CPPFLAGS} -o $@ $<

install: nmak
	mkdir -p ${PREFIX}/bin
	install -Dm755 $< ${PREFIX}/bin/$<

clean:
	rm -f nmak

distclean:
	rm -f nmak

.PHONY: install clean distclean
