
CFLAGS += -Wall

ifdef DEBUG
	CFLAGS += -fno-omit-frame-pointer -g -O0 -DDEBUG=1
else
	CFLAGS += -O2 -DDEBUG=0
endif

HEADERS = 
SRCS = main_dummy.c

ifeq ($(BUILD_OS),linux)
	TRIPLE = x86_64-linux-gnu
	CFLAGS += -std=gnu++11 -lrt
	HEADERS = bench.h
	SRCS = main_linux.cc ioutil.cc executil.cc bench.cc
endif

ifeq ($(BUILD_OS),darwin)
	TRIPLE = x86_64-apple-darwin14
endif

ifeq ($(BUILD_OS),windows)
	TRIPLE = x86_64-w64-mingw32
endif

BUILD_IMAGE = docker.io/steeve/cross-compiler

TARGETS = detectbatch.linux detectbatch.darwin detectbatch.windows

all: all-os

all-os: sub-linux sub-darwin sub-windows

sub-%:
	@$(MAKE) BUILD_OS=$* detectbatch.$*

detectbatch.%: $(SRCS) $(HEADERS)
	@docker run --rm -v $(CURDIR):/w $(BUILD_IMAGE):$*-x64 \
		$(TRIPLE)-c++ $(CFLAGS) $(addprefix /w/,$(SRCS)) -o /w/$@ 2>&1 \
		| sed "s|/w/|$(CURDIR)/|g"

fmt:
	@find . -iname '*.cc' -o -iname '*.h' -o -iname '*.c' | xargs clang-format -i

clean:
	rm -f $(TARGETS)
