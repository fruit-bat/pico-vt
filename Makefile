# Host build and run of the unit tests (the library itself is built by the
# Pico SDK through CMakeLists.txt).
#
#   make test             build and run every suite with ASan and UBSan
#   make harness          build the desktop harness, build-test/vt_harness
#                         (up to 132 columns; e.g. HARNESS_MAX=-DVT_SCREEN_MAX_COLS=80 for the default)
#   make test SANITIZE=   the same without the sanitizers
#   make clean

CC       ?= cc
SANITIZE ?= -fsanitize=address,undefined -fno-omit-frame-pointer
CFLAGS   ?= -Wall -Wextra -Werror -g
SRC      := src
BUILD    := build-test

TESTS := vt_cell vt_tabs vt_pallet vt_parser vt_utf8 vt_charmap vt_term vt_emu

# Sources for each suite (in src/)
vt_cell_SRCS    := vt_cell.c vt_cell_test.c
vt_tabs_SRCS    := vt_tabs.c vt_tabs_test.c
vt_pallet_SRCS  := vt_pallet.c vt_pallet_test.c
vt_parser_SRCS  := vt_parser.c vt_parser_test.c
vt_utf8_SRCS    := vt_utf8.c vt_utf8_test.c
vt_charmap_SRCS := vt_charmap.c vt_charmap_test.c
vt_term_SRCS    := vt_cell.c vt_tabs.c vt_term.c vt_term_test_utils.c vt_term_test.c
vt_emu_SRCS     := vt_cell.c vt_tabs.c vt_term.c vt_term_test_utils.c vt_parser.c \
                   vt_pallet.c vt_utf8.c vt_charmap.c vt_emu.c vt_emu_test.c

BINS := $(addprefix $(BUILD)/,$(addsuffix _test,$(TESTS)))

# The desktop harness (tools/vt_harness.c)
LIB_SRCS := vt_cell.c vt_tabs.c vt_term.c vt_parser.c vt_pallet.c vt_utf8.c vt_charmap.c vt_emu.c
HARNESS_LIBS := $(if $(filter Darwin,$(shell uname -s)),,-lutil)
# The harness's capacity, as a VT132-style 132 column terminal
HARNESS_MAX  ?= -DVT_SCREEN_MAX_COLS=132

.PHONY: test clean harness
.SECONDEXPANSION:

test: $(BINS)
	@set -e; for t in $(BINS); do \
		printf '%-32s' "$$t"; \
		$$t > $$t.log 2>&1 || { echo FAILED; tail -20 $$t.log; exit 1; }; \
		echo ok; \
	done

$(BUILD)/%_test: $$(addprefix $(SRC)/,$$($$*_SRCS)) $(wildcard $(SRC)/*.h) | $(BUILD)
	$(CC) $(CFLAGS) $(SANITIZE) -o $@ $(filter %.c,$^)

harness: $(BUILD)/vt_harness

$(BUILD)/vt_harness: tools/vt_harness.c $(addprefix $(SRC)/,$(LIB_SRCS)) $(wildcard $(SRC)/*.h) Makefile | $(BUILD)
	$(CC) $(CFLAGS) $(SANITIZE) $(HARNESS_MAX) -I$(SRC) -o $@ $(filter %.c,$^) $(HARNESS_LIBS)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
