include Make.defines

PROGS =	smdrreceiver
LIBS  = -L/usr/local/mysql/lib -lmysqlclient -lpthread -lm -lrt -ldl
CFLAGS = -I/usr/local/mysql/include

all:	${PROGS}

smdrreceiver:	smdrreceiver.o
		${CC} ${CFLAGS} -o $@ smdrreceiver.o ${LIBS}

clean:
		rm -f ${PROGS} ${CLEANFILES}
