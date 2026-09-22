# Host unit tests for the Air60 V2 port: compiles the real side.c and the
# common-layer config.c against stub QMK headers and runs test_air60v2.
QMK   ?= ../../../../..
BOARD  = $(QMK)/keyboards/nuphy/air60v2/ansi
NUPHY  = $(QMK)/keyboards/nuphy
BUILD  = build

CC      ?= cc
CFLAGS  += -std=gnu11 -O0 -g -Wall -Wno-unused-function -Wno-unknown-pragmas
CFLAGS  += -DMATRIX_ROWS=6 -DMATRIX_COLS=17 -DRGB_MATRIX_VAL_STEP=64
CFLAGS  += -Istubs -I$(NUPHY) -I$(QMK)/quantum -I$(QMK)/platforms -I$(BOARD)
CFLAGS  += -include $(BOARD)/config.h

OBJS = $(BUILD)/side.o $(BUILD)/usjis.o $(BUILD)/config.o $(BUILD)/color.o $(BUILD)/stubs.o $(BUILD)/test_air60v2.o
USJIS_OBJS = $(BUILD)/usjis.o $(BUILD)/config.o $(BUILD)/color.o $(BUILD)/stubs.o $(BUILD)/test_usjis.o

all: test

test: $(BUILD)/test_air60v2 $(BUILD)/test_usjis
	./$(BUILD)/test_air60v2
	./$(BUILD)/test_usjis

$(BUILD)/test_usjis: $(USJIS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(USJIS_OBJS)

$(BUILD)/usjis.o: $(NUPHY)/common/core/usjis.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/test_usjis.o: test_usjis.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/test_air60v2: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/side.o: $(BOARD)/side.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/config.o: $(NUPHY)/common/config/config.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/color.o: $(QMK)/quantum/color.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/stubs.o: stubs.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/test_air60v2.o: test_air60v2.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
