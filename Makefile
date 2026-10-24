# KarachiChal build (Linux / MinGW make)
#   make          -> build server   (build/karachichal)
#   make test     -> build + run tests
#   make run      -> build + run server on http://localhost:8080
#   make clean

CXX      ?= g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Ibackend/include -Ibackend

LIB_SRC    := $(wildcard backend/ds/*.cpp) $(wildcard backend/core/*.cpp)
SERVER_SRC := $(LIB_SRC) $(wildcard backend/api/*.cpp) backend/main.cpp
TEST_SRC   := $(LIB_SRC) tests/test_main.cpp

ifeq ($(OS),Windows_NT)
  EXT    := .exe
  LDLIBS := -static -lws2_32
else
  EXT    :=
  LDLIBS := -pthread
endif

SERVER_BIN := build/karachichal$(EXT)
TEST_BIN   := build/karachichal_tests$(EXT)

.PHONY: all server test run clean

all: server

server: $(SERVER_BIN)

$(SERVER_BIN): $(SERVER_SRC) $(wildcard backend/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o $@ $(LDLIBS)

$(TEST_BIN): $(TEST_SRC) $(wildcard backend/*/*.h)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $@ $(LDLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

run: $(SERVER_BIN)
	./$(SERVER_BIN)

clean:
	rm -rf build
