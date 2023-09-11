EXEC ?= riteg
LIBS := lib/gl.c lib/parson.c lib/stb.c

all: $(EXEC)

clean:
	rm -fv $(EXEC)

$(EXEC): src/riteg.c | $(LIBS)
	c99 $? $(LIBS) -o $@ -lm -lglfw -lz -g -Iinclude -Wall -Wextra -Werror -pedantic
