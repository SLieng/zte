VERSION = $(shell git describe --tags)
PREFIX = /usr/local
GTK = gtk+-3.0
VTE = vte-2.91
TERMINFO = ${PREFIX}/share/terminfo

srcdir=		.
OBJ := ./obj
MOD := ./mode

MODS := $(wildcard $(MOD)/*.c)
OBJS := $(patsubst $(MOD)/%.c,$(OBJ)/%.o,$(MODS))

CXXFLAGS := -std=c++11 -O3 \
	    -Wall -Wextra -pedantic \
	    -Winit-self \
	    -Wshadow \
	    -Wformat=2 \
	    -Wmissing-declarations \
	    -Wstrict-overflow=5 \
	    -Wcast-align \
	    -Wconversion \
	    -Wunused-macros \
	    -Wwrite-strings \
	    -fextended-identifiers \
	    -DNDEBUG \
	    -D_POSIX_C_SOURCE=200809L \
	    -DTERMITE_VERSION=\"${VERSION}\" \
	    ${shell pkg-config --cflags ${GTK} ${VTE}} \
	    ${CXXFLAGS}

ifeq (${CXX}, g++)
	CXXFLAGS += -Wno-missing-field-initializers
endif

ifeq (${CXX}, clang++)
	CXXFLAGS += -Wimplicit-fallthrough
endif

# LDFLAGS := -s -Wl,--as-needed ${LDFLAGS}
LDFLAGS := -s -Wl,-rpath,/usr/local/lib
LDLIBS := ${shell pkg-config --libs ${GTK} ${VTE}}

# CFILES := mode/a.c
# COBJ := mode/.c

# $(COBJ): mode/a.c
# 	${CXX} ${CXXFLAGS} ${LDFLAGS} $< ${LDLIBS} -c -o $@
ztermite: $(OBJS) ztermite.o
	${CXX} ${CXXFLAGS} ${LDFLAGS} $^ -o $@ ${LDLIBS}

$(OBJ)/%.o: $(MOD)/%.c | $(OBJ)
	${CXX} ${CXXFLAGS} ${LDFLAGS} ${LDLIBS} -c $< -o $@

ztermite.o: ztermite.cc ./mode/modes.hh
	${CXX} ${CXXFLAGS} ${LDFLAGS} ${LDLIBS} -c $< -o $@


install: ztermite ztermite.desktop ztermite.terminfo
	mkdir -p ${DESTDIR}${TERMINFO}
	install -Dm755 ztermite ${DESTDIR}${PREFIX}/bin/ztermite
	install -Dm644 config ${DESTDIR}/etc/xdg/ztermite/config
	tic -x -o ${DESTDIR}${TERMINFO} ztermite.terminfo

# uninstall:
# 	rm -f ${DESTDIR}${PREFIX}/bin/ztermite

clean:
	rm ztermite

.PHONY: clean install uninstall
