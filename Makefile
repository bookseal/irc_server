# Compiler and compiler flags
CC=c++
CFLAGS=-Wall -std=c++98 -pthread

# Program name
NAME=ircserv

# Source files
SRCS=main.cpp Server.cpp ClientSession.cpp

# Object files
OBJS=$(SRCS:.cpp=.o)

# Standard all target
all: $(NAME)

# Linking the program
$(NAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROGRAMNAME) $(OBJS)

# This rule tells make how to build .o files from .cpp files
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target - removes object files
clean:
	rm -f $(OBJS)

# Full Clean (fclean) target - removes object files and program
fclean: clean
	rm -f $(PROGRAMNAME)

# Rebuild target - makes fclean followed by all
re: fclean all

.PHONY: all clean fclean re
