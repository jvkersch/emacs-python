CFLAGS = -Wall -O0 -ggdb3

PY_CFLAGS := $$(python-config --cflags)
PY_LDFLAGS := $$(python-config --ldflags)
RPATH := $$(tools/find_rpath.py)

emacs-python.so: emacs_python.o
	$(CC) -shared $< -o $@ $(PY_LDFLAGS) $(LDFLAGS) -Wl,-rpath,$(RPATH)

%.o: %.c
	$(CC) $(PY_CFLAGS) $(CFLAGS) -I. -fPIC -c $<

.PHONY: clean tests

clean:
	rm -f emacs-python.so *.o
