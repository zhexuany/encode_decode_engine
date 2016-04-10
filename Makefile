TARGET = codec_4061
LIBS = -lm
CC = gcc
CFLAGS = -g -Wall
SRC_DIR = src
EXEC_DIR = build
.PHONY: default all clean

default: setup $(TARGET)
all: default

#Define the list of files to comiple for this project. defaults to all
# of the .cpp files in the source directory 
CFILES = $(wildcard $(SRC_DIR)/*.c)
INCLUDE = $(wildcard $(SRC_DIR)/*.h)
#For each of the .cpp files found above, deterine the name of
#the corresponding .o ile to create
OBJFILES = $(addprefix $(EXEC_DIR)/, $(notdir $(CFILES:.c=.o)))

setup: | $(EXEC_DIR)

$(EXEC_DIR):
	mkdir -p $(EXEC_DIR)
	
.PRECIOUS: $(TARGET)

$(TARGET): $(OBJFILES)
		$(CC) $(OBJFILES) -Wall $(LIBS) -o $@

#crreate all .o files in EXEC_DIR
$(EXEC_DIR)/%.o:  $(SRC_DIR)/%.c $(INCLUDE) 
		$(CC) -c $< -o $@

clean:
		-rm -f *.out
		-rm -rf $(EXEC_DIR)
		-rm -f $(TARGET)
