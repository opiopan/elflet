DEBUGFLAGS	= -g
CFLAGS		= -I../firmware/main $(DEBUGFLAGS) -std=c99
INSTALLDIR	= /usr/local/bin
TARGETS 	= irtx
OBJS		= irtx.o irslib.o

all: $(TARGETS)

irtx: $(OBJS)
	$(CC) -o $(LFLAGS) $@ $(OBJS)

install: all
	cp $(TARGETS)  $(INSTALLDIR)


clean:
	rm -f $(TARGETS) $(OBJS)
