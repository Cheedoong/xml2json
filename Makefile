# xml2json: a header only XML <-> JSON converter.
#
# This Makefile works with GNU make on POSIX systems and with mingw32-make on
# Windows.  The very same targets are available through CMake, see
# CMakeLists.txt:
#
#     cmake -S . -B build && cmake --build build && ctest --test-dir build

CXX      ?= c++
CXXFLAGS ?= -std=c++11 -O3 -fdata-sections -ffunction-sections
WARNINGS ?= -Wall -Wextra -Werror
INCLUDE  ?= -Iinclude

# Windows needs the .exe suffix and a different remove command.
ifeq ($(OS),Windows_NT)
  EXEEXT := .exe
  RM     := del /Q
  RUN    :=
else
  EXEEXT :=
  RM     := rm -f
  RUN    := ./
  UNAME  := $(shell uname -s)
  ifeq ($(UNAME),Darwin)
    LDFLAGS += -Wl,-search_paths_first -Wl,-dead_strip
  else
    LDFLAGS += -Wl,--gc-sections -Wl,--strip-all
  endif
endif

EXEC := xml2json$(EXEEXT)
TEST := test$(EXEEXT)

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
INSTALL ?= install

#############################################################

all: $(EXEC)

xml2json.o: xml2json.cpp include/xml2json.hpp
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(INCLUDE) -c $< -o $@

$(EXEC): xml2json.o
	$(CXX) $^ $(LDFLAGS) -o $@

# The test binary links two translation units that both include the header, so
# it does not build if the header-only functions regain external linkage
# (issue #23).
$(TEST): test/test.cpp test/test_link.cpp include/xml2json.hpp
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(INCLUDE) $^ $(LDFLAGS) -o $@

test: $(TEST)
	$(RUN)$(TEST)

install: $(EXEC)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 755 $(EXEC) "$(DESTDIR)$(BINDIR)"

uninstall:
	$(RM) "$(DESTDIR)$(BINDIR)/$(EXEC)"

clean:
	-$(RM) xml2json.o $(EXEC) $(TEST)

.PHONY: all test install uninstall clean
