APP      = ls-paint
VERSION  = 0.3-dev
PREFIX   = /usr/local

# Detect webkit2gtk version
WEBKIT_PKG := $(shell pkg-config --exists webkit2gtk-4.1 2>/dev/null && echo webkit2gtk-4.1 || echo webkit2gtk-4.0)
CFLAGS   = -O2 -Wall $(shell pkg-config --cflags gtk+-3.0 $(WEBKIT_PKG))
LDFLAGS  = $(shell pkg-config --libs gtk+-3.0 $(WEBKIT_PKG))

.PHONY: all clean install uninstall run

all: $(APP)

$(APP): src/main.c
	$(CC) $(CFLAGS) -o $@ src/main.c $(LDFLAGS)
	@echo "Built $(APP) ($(VERSION)) — $$(ls -lh $(APP) | awk '{print $$5}')"

run: $(APP)
	./$(APP)

clean:
	rm -f $(APP)

install: $(APP)
	install -Dm755 $(APP) $(DESTDIR)$(PREFIX)/bin/$(APP)
	install -Dm644 ls-paint.html $(DESTDIR)$(PREFIX)/share/ls-paint/ls-paint.html
	install -Dm644 ls-paint-icon.png $(DESTDIR)$(PREFIX)/share/ls-paint/ls-paint-icon.png
	install -Dm644 ls-paint-icon.png $(DESTDIR)$(PREFIX)/share/pixmaps/ls-paint.png
	install -Dm644 $(APP).desktop $(DESTDIR)$(PREFIX)/share/applications/$(APP).desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(APP)
	rm -rf $(DESTDIR)$(PREFIX)/share/ls-paint
	rm -f $(DESTDIR)$(PREFIX)/share/pixmaps/ls-paint.png
	rm -f $(DESTDIR)$(PREFIX)/share/applications/$(APP).desktop
