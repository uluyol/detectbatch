
CFLAGS += -Wall

ifdef DEBUG
	CFLAGS += -fno-omit-frame-pointer -g -O0 -DDEBUG=1
else
	CFLAGS += -O2 -DDEBUG=0
endif

SRCS = main_dummy.c

ifeq ($(BUILD_OS),linux)
	TRIPLE = x86_64-linux-gnu
	CFLAGS += -std=gnu++11
	SRCS = main_linux.cc ioutil.cc executil.cc
endif

ifeq ($(BUILD_OS),darwin)
	TRIPLE = x86_64-apple-darwin14
endif

ifeq ($(BUILD_OS),windows)
	TRIPLE = x86_64-w64-mingw32
endif

BUILD_IMAGE = docker.io/steeve/cross-compiler

TARGETS = detect_bench.linux detect_bench.darwin detect_bench.windows

all: all-os

all-os: sub-linux sub-darwin sub-windows

sub-%:
	@$(MAKE) BUILD_OS=$* detect_bench.$*

detect_bench.%: $(SRCS)
	@docker run --rm -v $(CURDIR):/w $(BUILD_IMAGE):$*-x64 \
		$(TRIPLE)-c++ $(CFLAGS) $(addprefix /w/,$(SRCS)) -o /w/$@ 2>&1 \
		| sed "s|/w/|$(CURDIR)/|g"

fmt:
	@find . -iname '*.cc' -o -iname '*.h' -o -iname '*.c' | xargs clang-format -i

clean:
	rm -f $(TARGETS)
