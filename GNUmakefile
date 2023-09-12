EXEC ?= riteg
LIBS := src/gl.c src/parson.c src/stb.c

all: $(EXEC)

clean:
	rm -fv $(EXEC)

$(EXEC): src/riteg.c | $(LIBS)
	c99 $? $(LIBS) -o $@ -lm -lglfw -O2 -Iinclude -Wall -Wextra -Werror -pedantic
