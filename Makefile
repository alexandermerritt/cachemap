SRCS=pageset.c probe.c timestats.c cachemap.c
PROJ=cachemap
CFLAGS=-std=gnu99 -g
LDLIBS=
LDFLAGS=

OBJS=$(SRCS:.c=.o)


all: $(PROJ)


cachemap: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

pageset.o: pageset.h

probe.o: probe.h pageset.h sysinfo.h

timestats.o: timestats.h

cachemap.o: timestats.h probe.h

evictionset.o: evictionset.h pageset.h probe.h

clean:
	rm -f $(PROJ) $(OBJS)
