all:
	make -C src

clean:
	make -C src $@

distclean: clean
	rm -f config.mk
