# Compiler tools
CC ?= gcc
LD ?= gcc
STRIP ?= strip

# Other tools
RM=rm

# Flags
CFLAGS=-Wall -O0 -g -DDEBUG -fPIC -rdynamic
LDFLAGS=-rdynamic
LIBS=-lm -lpthread -ldl
STRIPFLAGS=-s

PROJECT=slsp
SRC_DIR=src
SRCS=$(wildcard ${SRC_DIR}/*.c)
OBJS=${SRCS:%.c=%.o}

include Linux.mk

all: ${OBJS}
	@${MUDA_COR} ${AMARELO}
	@echo -n "Linking executable...  "
	@${RESETA_COR}
	@${CC} ${CFLAGS} ${LIBS} ${LDFLAGS} ${OBJS} -o ${PROJECT}
	@${MUDA_COR} ${VERDE}
	@echo "Ok."
	@${MUDA_COR} ${AMARELO}
	@echo -n "Stripping  executable...  "
	@${STRIP} ${STRIPFLAGS} ${PROJECT}
	@${MUDA_COR} ${VERDE}
	@echo "Ok."
	@${RESETA_COR}

${SRC_DIR}/%.o: ${SRC_DIR}/%.c
	@${MUDA_COR} ${AMARELO}
	@echo "Compiling $^...  "
	@${RESETA_COR}
	@${CC} -c ${CFLAGS} $^ -o $@
	@${MUDA_COR} ${VERDE}
	@echo "Ok. Output is in $@."
	@${RESETA_COR}

docs:
	@${MUDA_COR} ${AMARELO}
	@echo "Generating docs...  "
	@${RESETA_COR}
	@${DOXYGEN} doc/Doxyfile
	@${MUDA_COR} ${VERDE}
	@echo "Ok. Output is in doc/output/index.html"
	@${RESETA_COR}

install: all
	install -d $(DESTDIR)$(sysconfdir) $(DESTDIR)$(bindir)
	install -m 0755 slsp $(DESTDIR)$(bindir)
	install -m 0644 slsp.conf $(DESTDIR)$(sysconfdir)
	install -m 0644 perTable.csv $(DESTDIR)$(sysconfdir)

clean: 
	@${MUDA_COR} ${VERMELHO}
	@echo -n "Removing object files...  "
	@${RESETA_COR}
	@${RM} -f ${OBJS}
	@${MUDA_COR} ${VERMELHO}
	@echo "Ok."
	@echo -n "Removing executable file...  "
	@${RESETA_COR}
	@${RM} -f ${PROJECT}
	@${MUDA_COR} ${VERMELHO}
	@echo "Ok."
	@${RESETA_COR}

