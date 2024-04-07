
NAME		= ircserv
SRCS		= main.cpp

OBJS		= $(SRCS:%.cpp=%.o)
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98
 CXXFLAGS	+= -g3

RM			+= -f
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY:		all clean fclean re

all:		$(NAME)

clean:
			$(RM) $(OBJS)

fclean:
			make clean
			$(RM) $(NAME)

re:	fclean
	$(MAKE) all
