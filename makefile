CXX = g++
CPPFLAGS = -std=c++11 -Wall -g

SRCS_CPPS = \
				./src/main.cpp \

run:
	$(CXX) $(SRCS_CPPS) -o application $(CPPFLAGS)