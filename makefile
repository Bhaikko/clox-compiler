CXX = g++
CPPFLAGS = -std=c++11 -Wall -g

LIBS_CPPS = \
				./lib/chunk.c \
				./lib/memory.c \

SRCS_CPPS = \
				./src/main.cpp \

run:
	$(CXX) $(SRCS_CPPS) -o application $(CPPFLAGS)