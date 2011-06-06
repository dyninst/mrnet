CC      = cc
CFLAGS  = -g -O0
LDFLAGS = -g -O0

APP     = xt_testapp
SRCS    = xt_testapp.c
OBJS    = ${SRCS:.c=.o}
LIBS    =

all: ${APP}

${APP}: ${OBJS}
	${CC} -o $@ ${LDFLAGS} $^ ${LIBS}

install: all
	cp ${APP} ${INSTALL_BIN}/

clean:
	/bin/rm -f ${APP} ${OBJS}

