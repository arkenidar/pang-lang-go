# Pang C++17 interpreter build
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
TARGET := pang
SRCS := main.cpp interpreter.cpp
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp interpreter.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	@echo "=== factorial (italian) ==="
	@echo "fattoriale 5" | ./$(TARGET) italian ../tests/fattoriale.parole -
	@echo ""
	@echo "=== factorial (english) ==="
	@echo "factorial 5" | ./$(TARGET) ../tests/factorial.words -