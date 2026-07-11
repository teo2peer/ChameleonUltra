CC ?= cc
BUILD_DIR ?= build
SANITIZE ?= 0

APP_SRC := ../application/src
LF_UTILS := $(APP_SRC)/rfid/nfctag/lf/utils
LF_PROTOCOLS := $(APP_SRC)/rfid/nfctag/lf/protocols
HF_READER := $(APP_SRC)/rfid/reader/hf

CPPFLAGS += \
	-Istubs \
	-I../common \
	-I$(APP_SRC) \
	-I$(APP_SRC)/utils \
	-I$(APP_SRC)/rfid \
	-I$(APP_SRC)/rfid/nfctag \
	-I$(APP_SRC)/rfid/nfctag/lf \
	-I$(HF_READER) \
	-I$(LF_UTILS) \
	-I$(LF_PROTOCOLS)
CFLAGS += -std=c11 -O1 -g -Wall -Wextra -Werror -Wno-sign-compare -Wno-unused-parameter

ifeq ($(SANITIZE),1)
CFLAGS += -fno-omit-frame-pointer -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif

TESTS := \
	$(BUILD_DIR)/test_crc_utils \
	$(BUILD_DIR)/test_circular_buffer \
	$(BUILD_DIR)/test_dataframe \
	$(BUILD_DIR)/test_iso_dep_reader \
	$(BUILD_DIR)/test_lf_helpers

.PHONY: all test clean

all: $(TESTS)

test: all
	@set -e; for test_binary in $(TESTS); do \
		echo "Running $$test_binary"; \
		"$$test_binary"; \
	done

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_crc_utils: test_crc_utils.c $(APP_SRC)/rfid/crc_utils.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_circular_buffer: test_circular_buffer.c $(LF_UTILS)/circular_buffer.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_dataframe: test_dataframe.c $(APP_SRC)/utils/dataframe.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_iso_dep_reader: test_iso_dep_reader.c $(HF_READER)/iso_dep_reader.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_lf_helpers: \
		test_lf_helpers.c \
		$(LF_UTILS)/manchester.c \
		$(LF_UTILS)/diphase.c \
		$(LF_PROTOCOLS)/wiegand.c \
		$(LF_PROTOCOLS)/jablotron.c \
		$(APP_SRC)/rfid/parity.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)
