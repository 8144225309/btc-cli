# btc-cli Makefile
# Pure C, no external dependencies

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS =

# Source files
SRCS = btc-cli.c config.c methods.c rpc.c json.c
OBJS = $(SRCS:.c=.o)
HEADERS = config.h methods.h rpc.h json.h

# Output binary
TARGET = btc-cli

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Compile
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

# Install to /usr/local/bin
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Test connection to signet
test-signet: $(TARGET)
	./$(TARGET) -signet getblockchaininfo

# Test connection to local regtest
test-regtest: $(TARGET)
	./$(TARGET) -regtest getblockchaininfo

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: clean $(TARGET)

# Show help
help:
	@echo "Targets:"
	@echo "  all          Build btc-cli (default)"
	@echo "  clean        Remove build artifacts"
	@echo "  install      Install to /usr/local/bin"
	@echo "  uninstall    Remove from /usr/local/bin"
	@echo "  test-signet  Test connection to signet"
	@echo "  test-regtest Test connection to regtest"
	@echo "  debug        Build with debug symbols"

.PHONY: all clean install uninstall test-signet test-regtest debug help
