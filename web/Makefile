EMCC    = emcc
CFLAGS  = -O2 -Wall -Wextra
SHELL_  = shell.html
EXPORTS = -s EXPORTED_FUNCTIONS='["_main","_rb__web_audio_fill"]' \
          -s EXPORTED_RUNTIME_METHODS='["ccall"]'

EDITOR_EXPORTS = -s EXPORTED_FUNCTIONS='["_main","_malloc","_free","_editor_init","_editor_get_buffer","_editor_apply_effect","_editor_present","_editor_width","_editor_height","_editor_draw_text","_editor_draw_rect"]' \
                 -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","stringToUTF8"]' \
                 -s MODULARIZE=1 -s EXPORT_NAME='RawBuf' -s ALLOW_MEMORY_GROWTH=1

all: plasma.html piano.html editor.js

plasma.html: plasma.c ../rawbuf.h $(SHELL_)
	$(EMCC) $(CFLAGS) -o $@ $< --shell-file $(SHELL_) $(EXPORTS)

piano.html: piano.c ../rawbuf.h $(SHELL_)
	$(EMCC) $(CFLAGS) -o $@ $< --shell-file $(SHELL_) $(EXPORTS)

editor.js: editor.c ../rawbuf.h
	$(EMCC) $(CFLAGS) -o $@ $< $(EDITOR_EXPORTS)

serve: all
	python3 -m http.server 8080

clean:
	rm -f plasma.html plasma.js plasma.wasm
	rm -f piano.html piano.js piano.wasm
	rm -f editor.js editor.wasm

.PHONY: all clean serve
