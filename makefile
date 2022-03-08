CXX = g++
CPPFLAGS = -std=c++11 -Wall -g

UTILITY_CPPS = \
				./lib/debug.c \

LIBS_CPPS = \
				./lib/memory.c \
				./lib/chunk.c \

SRCS_CPPS = \
				./src/main.cpp \

run:
	$(CXX) $(UTILITY_CPPS) $(LIBS_CPPS) $(SRCS_CPPS) -o application $(CPPFLAGS)