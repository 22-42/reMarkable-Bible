# Makefile for the AppLoad C++ backend.
# C++ port of backends/appload-clients/rust-backend from asivery/rm-appload.
#
# Targets:
#   make        native build for the PC emulator   -> ./entry
#   make rm     cross build for rM1/rM2 (armv7)     -> ./entry
#   make rmpp   cross build for rMPP (aarch64)      -> ./entry
#   make clean
#
# The build-*.sh scripts in the parent directory copy ./entry to
# <output>/backend/entry.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

TARGET := entry
SRC    := $(wildcard src/*.cpp)
HDR    := $(wildcard src/*.h)

all: $(TARGET)

$(TARGET): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

# reMarkable 1/2: armv7 hard-float with NEON/VFPv4, like the rustflags
# (+neon,+vfp4) in the .cargo/config.toml of the Rust example.
# libstdc++/libgcc are linked statically so the binary does not depend on a
# specific libstdc++ version on the device.
rm: CXX      := arm-linux-gnueabihf-g++
rm: CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -static-libstdc++ -static-libgcc
rm: $(TARGET)

# reMarkable Paper Pro: aarch64 (glibc), same target triple as the Rust example.
rmpp: CXX      := aarch64-linux-gnu-g++
rmpp: CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -static-libstdc++ -static-libgcc
rmpp: $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all rm rmpp clean
