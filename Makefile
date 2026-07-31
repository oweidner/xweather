CC       = cc
CFLAGS   = -Wall -Wextra -g  -I/usr/include/x86_64-linux-gnu  -I/usr/include/json-c 
LDFLAGS  = 
LIBS     = -lXm -lXt -lX11 -lcurl  -ljson-c  -lXpm -lpthread
PREFIX   = /usr/local

SRCDIR   = src
BUILDDIR = build
TARGET   = xweather

SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
DEPS     = $(OBJECTS:.o=.d)

.PHONY: all clean distclean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

-include $(DEPS)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

distclean: clean
	rm -f Makefile config.log

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
