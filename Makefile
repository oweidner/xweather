CC       = cc
CFLAGS   = -Wall -Wextra -g   -I/usr/include/x86_64-linux-gnu  -I/usr/include/json-c 
LDFLAGS  = 
LIBS     = -lXm -lXt -lX11 -lcurl  -ljson-c   -lXpm -lpthread
PREFIX   = /usr/local

SRCDIR   = src
BUILDDIR = build
TARGET   = xweather

SOURCES  = $(wildcard $(SRCDIR)/*.c)
OBJECTS  = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SOURCES))
DEPS     = $(OBJECTS:.o=.d)

TESTDIR      = tests
TESTSRCS     = $(wildcard $(TESTDIR)/test_*.c)
TESTBINS     = $(patsubst $(TESTDIR)/%.c,$(BUILDDIR)/%,$(TESTSRCS))
NONMAIN_OBJS = $(filter-out $(BUILDDIR)/main.o,$(OBJECTS))

.PHONY: all test clean distclean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

-include $(DEPS)

# Each test binary links the test file's own main() against every app
# object except main.o (so it gets whatever module(s) it needs plus
# anything they in turn depend on, e.g. locations.o/weather_client.o both
# need model.o's placeholder-fill functions) -- simpler and more robust
# than trying to track a per-test dependency list by hand.
test: $(TESTBINS)
	@for t in $(TESTBINS); do echo "-- $$t --"; $$t || exit 1; done

$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(NONMAIN_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $< $(NONMAIN_OBJS) $(LIBS)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

distclean: clean
	rm -f Makefile config.log

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
