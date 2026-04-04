.PHONY: all app tests runtime clean

SHELL := cmd.exe
.SHELLFLAGS := /C

PROJECT := m3d
BUILD_DIR := build
APP_BIN := $(BUILD_DIR)/$(PROJECT).exe
TEST_BIN := $(BUILD_DIR)/m3d_tests.exe

CC ?= gcc
CXX ?= g++
SDL_ROOT ?= SDL3-3.2.22/x86_64-w64-mingw32
ASSIMP_ROOT ?= assimp
UCRT_BIN ?= C:/msys64/ucrt64/bin

CPPFLAGS += -DGUID_WINDOWS \
	-Iinclude \
	-Isrc \
	-Isrc/audio \
	-Isrc/ecs \
	-I$(SDL_ROOT)/include \
	-Iwren-0.4.0/src/include
CFLAGS ?= -std=c11 -Wall -Wextra
CXXFLAGS ?= -std=c++17 -Wall -Wextra -static-libstdc++ -static-libgcc
DEPFLAGS := -MMD -MP
LDFLAGS += -L$(SDL_ROOT)/lib -L$(ASSIMP_ROOT)/lib
LDLIBS += -lSDL3 -lassimp -lz -lole32

rwildcard = $(foreach d,$(wildcard $(1)/*),$(call rwildcard,$(d),$(2)) $(filter $(subst *,%,$(2)),$(d)))

APP_CPP_SOURCES := $(call rwildcard,src,*.cpp)
APP_C_SOURCES := $(call rwildcard,src,*.c)
TEST_SUPPORT_SOURCES := \
	src/guid.cpp \
	src/ecs/ecs.cpp \
	src/audio/audio.cpp \
	src/input.cpp \
	src/script_manager.cpp \
	src/rendering/camera.cpp \
	src/ecs/audio_player_component.cpp \
	src/ecs/mesh_renderer_component.cpp \
	src/ecs/script_component.cpp
TEST_SOURCES := $(wildcard tests/*.cpp)
WREN_SOURCES := $(wildcard wren-0.4.0/src/vm/*.c)
RUNTIME_DLLS := \
	$(BUILD_DIR)/SDL3.dll
GENERATED_FILES := $(APP_BIN) $(TEST_BIN) $(RUNTIME_DLLS)

APP_CPP_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/obj/app/cpp/%.o,$(APP_CPP_SOURCES))
APP_C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/obj/app/c/%.o,$(APP_C_SOURCES))
STB_IMAGE_OBJECT := $(BUILD_DIR)/obj/app/cpp/stb_image_impl.o
APP_OBJECTS := $(APP_CPP_OBJECTS) $(APP_C_OBJECTS) $(STB_IMAGE_OBJECT)
TEST_SUPPORT_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/obj/test_support/%.o,$(TEST_SUPPORT_SOURCES))
TEST_OBJECTS := $(patsubst tests/%.cpp,$(BUILD_DIR)/obj/tests/%.o,$(TEST_SOURCES))
WREN_OBJECTS := $(patsubst wren-0.4.0/src/vm/%.c,$(BUILD_DIR)/obj/wren/%.o,$(WREN_SOURCES))

APP_CPP_DEPS := $(APP_CPP_OBJECTS:.o=.d)
APP_C_DEPS := $(APP_C_OBJECTS:.o=.d)
STB_IMAGE_DEPS := $(STB_IMAGE_OBJECT:.o=.d)
TEST_SUPPORT_DEPS := $(TEST_SUPPORT_OBJECTS:.o=.d)
TEST_DEPS := $(TEST_OBJECTS:.o=.d)
WREN_DEPS := $(WREN_OBJECTS:.o=.d)

all: app tests

app: $(APP_BIN) runtime

tests: $(TEST_BIN) runtime

$(APP_BIN): $(APP_OBJECTS) $(WREN_OBJECTS)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(TEST_BIN): $(TEST_SUPPORT_OBJECTS) $(TEST_OBJECTS) $(WREN_OBJECTS)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/obj/app/cpp/%.o: src/%.cpp
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/obj/app/c/%.o: src/%.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(STB_IMAGE_OBJECT): include/stb_image.h
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -DSTB_IMAGE_IMPLEMENTATION -x c++ -c $< -o $@

$(BUILD_DIR)/obj/test_support/%.o: %.cpp
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/obj/tests/%.o: tests/%.cpp
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/obj/wren/%.o: wren-0.4.0/src/vm/%.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

runtime: $(RUNTIME_DLLS)

$(BUILD_DIR)/SDL3.dll: $(SDL_ROOT)/bin/SDL3.dll
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	copy /Y "$(subst /,\,$<)" "$(subst /,\,$@)" >NUL

clean:
	@if exist "$(subst /,\,$(BUILD_DIR)/obj)" rmdir /S /Q "$(subst /,\,$(BUILD_DIR)/obj)"
	@for %%F in ($(subst /,\,$(GENERATED_FILES))) do @if exist "%%F" del /Q "%%F"

-include $(APP_CPP_DEPS) $(APP_C_DEPS) $(STB_IMAGE_DEPS) $(TEST_SUPPORT_DEPS) $(TEST_DEPS) $(WREN_DEPS)
