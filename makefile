CXX = g++
CPPFLAGS = -std=c++11 -Wall -g

UTILITY_CPPS = \
				./lib/debug.c \

LIBS_CPPS = \
				./lib/value.c \
				./lib/object.c \
				./lib/memory.c \
				./lib/chunk.c \
				./lib/table.c \
				./lib/vm.c \
				./lib/scanner.c \
				./lib/compiler.c \

SRCS_CPPS = \
				./src/main.cpp \

run:
	$(CXX) $(UTILITY_CPPS) $(LIBS_CPPS) $(SRCS_CPPS) -o clox $(CPPFLAGS)