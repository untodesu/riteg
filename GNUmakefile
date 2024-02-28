CFLAGS += -Wall -Wextra -pedantic 
CFLAGS += -D GL_GLEXT_PROTOTYPES
CFLAGS += -I src -O2
LDFLAGS += -lGL -lm -lglfw

C99 ?= c99

all: riteg

clean:
	rm -fv riteg

riteg: src/parson.c src/riteg.c src/stb_impl.c
	${C99} $^ -o $@ ${CFLAGS} ${LDFLAGS}
