CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude -MMD -MP
TARGET   := shell

SRCS := $(wildcard src/*.cpp src/builtins/*.cpp)
OBJS := $(SRCS:src/%.cpp=build/%.o)
DEPS := $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Debug build: catches memory errors and undefined behaviour at runtime
debug: CXXFLAGS += -g -O0 -fsanitize=address,undefined
debug: clean $(TARGET)

clean:
	rm -rf build $(TARGET)

-include $(DEPS)

.PHONY: all clean debug
