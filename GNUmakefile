EXEC ?= riteg

all: $(EXEC)

clean:
	rm -fv $(EXEC)

$(EXEC): src/riteg.c src/lib/gl.c src/lib/parson.c src/lib/stb.c
	c99 $? -o $@ -lm -lglfw -Iinclude -Wall -Wextra -Werror -pedantic
