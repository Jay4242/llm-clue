
# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -std=c++11

# Libraries
LIBS = -lcurl -lcpprest -lpthread -lz -lcrypto -lssl

# Source files
CLUE_SRC = clue.cpp
EASY_DIFFUSION_SRC = easy_diffusion.cpp

# Executable names
CLUE_EXEC = clue
EASY_DIFFUSION_EXEC = easy_diffusion

# Default target
all: $(CLUE_EXEC) $(EASY_DIFFUSION_EXEC)

# Rule to compile clue.cpp
$(CLUE_EXEC): $(CLUE_SRC)
	$(CXX) $(CXXFLAGS) $(CLUE_SRC) -o $(CLUE_EXEC) -lcurl

# Rule to compile easy_diffusion.cpp
$(EASY_DIFFUSION_EXEC): $(EASY_DIFFUSION_SRC)
	$(CXX) $(CXXFLAGS) $(EASY_DIFFUSION_SRC) -o $(EASY_DIFFUSION_EXEC) -lcpprest -lpthread -lz -lcrypto -lssl

# Clean target to remove executables
clean:
	rm -f $(CLUE_EXEC) $(EASY_DIFFUSION_EXEC)

# Install target (optional)
install:
	mkdir -p /usr/local/bin
	cp $(CLUE_EXEC) /usr/local/bin
	cp $(EASY_DIFFUSION_EXEC) /usr/local/bin

# Phony targets
.PHONY: all clean install
