sources  = $(wildcard *.cc)
builddir = ./build
objects  = $(addprefix $(builddir)/,$(sources:.cc=.o))
depends  = $(objects:.o=.d)
pkgs     = tinyxml2 absl_strings cairo xcb cairo-pdf xcb-icccm
flags    = $(shell pkg-config --cflags $(pkgs))
libs     = $(shell pkg-config --libs $(pkgs))

all: main image-a4.h image-a3.h

GENFLAGS=

image-a4.h: Makefile main input/example.svg
	echo >image-a4.h
	./main input/example.svg -a4 $(GENFLAGS) -name Example >>image-a4.h

image-a3.h: Makefile main input/example.svg
	echo >image-a3.h
	./main input/example.svg $(GENFLAGS) -name Example >>image-a3.h

main: $(objects)
	mkdir -p $(builddir)
	g++ -O3 -g -std=c++20 $(flags) $(objects) -o $@ $(libs)

$(builddir)/%.d: %.cc
	mkdir -p $(builddir)
	echo -n $@" "$(builddir)/> $@
	g++ $(flags) -MM $< >> $@

include $(depends)

$(builddir)/%.o: %.cc
	mkdir -p $(builddir)
	g++ $(flags) -O3 -g -std=c++20 -c $< -o $@

PHONY.: clean
clean:
	rm -rf $(builddir)
	rm -f main
