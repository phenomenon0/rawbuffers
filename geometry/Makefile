CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation
LDFLAGS = -lm -lpthread

SRCS = main.c demos_1.c demos_2.c demos_3.c demos_4.c
OBJS = $(SRCS:.c=.o)
TARGET = geometry

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.c ../rawbuf.h geometry_math.h demos.h
	$(CC) $(CFLAGS) -c main.c

demos_1.o: demos_1.c geometry_math.h demos.h
	$(CC) $(CFLAGS) -c demos_1.c

demos_2.o: demos_2.c geometry_math.h demos.h
	$(CC) $(CFLAGS) -c demos_2.c

demos_3.o: demos_3.c geometry_math.h demos.h
	$(CC) $(CFLAGS) -c demos_3.c

demos_4.o: demos_4.c geometry_math.h demos.h
	$(CC) $(CFLAGS) -c demos_4.c

clean:
	rm -f $(OBJS) $(TARGET) geometry.html geometry.js geometry.wasm

# ─── WASM build (requires emcc) ───
WASM_SRCS = geometry_web.c demos_1.c demos_2.c demos_3.c demos_4.c

wasm: geometry.html

geometry.html: $(WASM_SRCS) geometry_math_web.h demos.h shell.html
	emcc -O2 -DRAWBUF_GEOMETRY \
		-Wno-unused-parameter -Wno-misleading-indentation \
		-o geometry.html $(WASM_SRCS) \
		-s 'EXPORTED_FUNCTIONS=["_render_frame","_get_buffer","_set_demo","_set_mouse","_get_width","_get_height","_set_resolution","_get_num_categories","_get_category_count","_get_category_label","_get_category_thesis","_get_item_title","_get_item_id","_get_current_cat","_get_current_item","_set_time_scale","_get_time_scale","_set_auto_rotate","_get_auto_rotate","_main"]' \
		-s 'EXPORTED_RUNTIME_METHODS=["ccall","cwrap","UTF8ToString","HEAPU8"]' \
		-s ALLOW_MEMORY_GROWTH=0 \
		-s INITIAL_MEMORY=8388608 \
		--shell-file shell.html

.PHONY: all clean wasm
