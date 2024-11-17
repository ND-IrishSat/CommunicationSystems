COMP = gcc
EXEC = main_executable
INCLUDE =
LINK =
LIBS = -lm
REMOVE_GRAPHS = lib/graphs/*.txt

$(EXEC) : main.c
	$(COMP) main.c -o $(EXEC) -I $(INCLUDE) -L $(LINK) $(LIBS)
clean :
	-rm $(EXEC)
	-rm $(REMOVE_GRAPHS)
all :
	make $(TRANS)