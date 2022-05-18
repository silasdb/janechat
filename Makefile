all:
	make -C src

clean:
	make -C src $@

.PHONY: test check
check: test
test:
	make -C src test-ui-curses
	make -C tests

distclean: clean
	rm -f config.mk
