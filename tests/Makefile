.PHONY: test check
check: unit
test: unit
all: unit ui

.PHONY: unit
unit:
	@echo '==> Running unit tests...'
	$(MAKE) -C $@

.PHONY: ui
ui:
	@echo '==> Running UI tests...'
	$(MAKE) -C $@

.PHONY: clean
clean:

	$(MAKE) -C unit clean
	$(MAKE) -C ui clean
