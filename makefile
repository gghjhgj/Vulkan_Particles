SOURCES := $(shell find . -name "*.cpp")
TARGET = app

CXX = g++
CLANG = clang++

GLSLC = glslc
SHADER_SOURCES := $(shell find . -maxdepth 1 -name "*.glsl") $(shell find shaders -name "*.glsl" 2>/dev/null)
SHADER_OBJECTS = $(SHADER_SOURCES:.glsl=.spv)
ifdef VULKAN_SDK
    VULKAN_DIR = $(subst \,/,$(VULKAN_SDK))
    VULKAN_INC = -I"$(VULKAN_DIR)/Include"
    VULKAN_LD  = -L"$(VULKAN_DIR)/Lib" -lvulkan-1
else
    VULKAN_INC =
    VULKAN_LD  = -lvulkan-1
endif

LIBS = -lsfml-graphics -lsfml-window -lsfml-system -lmimalloc $(VULKAN_LD)
CLANG_LIBS = -lsfml-graphics -lsfml-window -lsfml-system $(VULKAN_LD)

INCLUDE = -I. $(VULKAN_INC)

COMMON_FLAGS = -march=native -flto -Ofast -funroll-loops -ffast-math

CLANG_FLAGS = -O2 -march=native -fno-omit-frame-pointer \
-Wno-c++11-narrowing -g -gcodeview -fuse-ld=lld \
-Wl,-pdb=app.pdb

GCC_PROFILE_DIR = profiles/gcc
CLANG_PROFILE = profiles/clang/default.profraw
CLANG_PROFDATA = profiles/clang/app.profdata

%_vert.spv: %_vert.glsl
	@echo "Kompilacja shadera Vertex: $< -> $@"
	$(GLSLC) -fshader-stage=vertex $< -o $@

%_frag.spv: %_frag.glsl
	@echo "Kompilacja shadera Fragment: $< -> $@"
	$(GLSLC) -fshader-stage=fragment $< -o $@

%.spv: %.glsl
	@echo "Kompilacja shadera Compute: $< -> $@"
	$(GLSLC) -fshader-stage=compute $< -o $@

shader: $(SHADER_OBJECTS)
	@echo "Shadery skompilowane."

release: CXXFLAGS = $(COMMON_FLAGS)
release: $(SHADER_OBJECTS) build
	@echo "Build RELEASE gotowy."

profile: CXXFLAGS = -O2 -march=native -g -fno-omit-frame-pointer
profile: $(SHADER_OBJECTS) build
	@echo "Build PROFILE gotowy."

debug: CXXFLAGS = -g -O0 -Wall -Wextra
debug: $(SHADER_OBJECTS) build
	@echo "Build DEBUG gotowy."

generate: CXXFLAGS = $(COMMON_FLAGS) -fprofile-generate=$(GCC_PROFILE_DIR)
generate: $(SHADER_OBJECTS)
	@mkdir -p $(GCC_PROFILE_DIR)
	@echo "Generowanie GCC PGO..."
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(SOURCES) -o $(TARGET).exe $(LIBS)
	@echo ""
	@echo "Odpal app.exe i wykonaj testy."
	@echo "Profile zapiszą się w $(GCC_PROFILE_DIR)"

use: CXXFLAGS = $(COMMON_FLAGS) -fprofile-use=$(GCC_PROFILE_DIR) -fprofile-correction
use: $(SHADER_OBJECTS)
	@echo "Używanie GCC PGO..."
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(SOURCES) -o $(TARGET).exe $(LIBS)
	@echo "Build GCC PGO gotowy."

build:
	@echo "Kompilowanie..."
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(SOURCES) -o $(TARGET).exe $(LIBS)

clang: $(SHADER_OBJECTS)
	@echo "Build CLANG..."
	$(CLANG) $(CLANG_FLAGS) \
	$(INCLUDE) \
	$(SOURCES) \
	-o $(TARGET).exe \
	$(CLANG_LIBS)
	@echo "Build CLANG gotowy."

clang-generate: $(SHADER_OBJECTS)
	@mkdir -p profiles/clang
	@echo "Generowanie CLANG PGO..."
	$(CLANG) $(CLANG_FLAGS) \
	-fprofile-instr-generate=$(CLANG_PROFILE) \
	$(INCLUDE) \
	$(SOURCES) \
	-o $(TARGET).exe \
	$(CLANG_LIBS)
	@echo ""
	@echo "Odpal app.exe aby wygenerować:"
	@echo "$(CLANG_PROFILE)"

clang-use: $(SHADER_OBJECTS)
	@echo "Tworzenie profilu LLVM..."
	llvm-profdata merge \
	-output=$(CLANG_PROFDATA) \
	$(CLANG_PROFILE)

	@echo "Budowanie CLANG z PGO..."
	$(CLANG) $(CLANG_FLAGS) \
	-fprofile-instr-use=$(CLANG_PROFDATA) \
	$(INCLUDE) \
	$(SOURCES) \
	-o $(TARGET).exe \
	$(CLANG_LIBS)
	@echo "Build CLANG PGO gotowy."

clean:
	rm -f $(TARGET).exe
	rm -rf profiles
	rm -f *.pdb
	find . -name "*.spv" -delete
	@echo "Czyszczenie zakończone."
