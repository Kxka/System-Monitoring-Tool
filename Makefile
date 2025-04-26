CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -Werror -D_POSIX_C_SOURCE=200809L

TARGET=showFDtables

$(TARGET): $(TARGET).o 
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).o  
$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -c $(TARGET).c

.PHONY: clean  
clean:
	rm -f $(TARGET) $(TARGET).o compositeTable.txt compositeTable.bin 

.PHONY: help
help:
	@echo "make: Compile program"
	@echo "make clean: Remove files"
