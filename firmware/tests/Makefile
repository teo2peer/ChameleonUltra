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
	-I$(APP_SRC)/bsp \
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
	$(BUILD_DIR)/test_hf_capture \
	$(BUILD_DIR)/test_active_slot_snapshot \
	$(BUILD_DIR)/test_device_settings_payload \
	$(BUILD_DIR)/test_keyboard_payload \
	$(BUILD_DIR)/test_hf_memory_safety \
	$(BUILD_DIR)/test_iso_dep_reader \
	$(BUILD_DIR)/test_iso_dep_session \
	$(BUILD_DIR)/test_lf_helpers \
	$(BUILD_DIR)/test_tag_persistence

.PHONY: all test clean

all: $(TESTS)

test: all
	@set -e; for test_binary in $(TESTS); do \
		echo "Running $$test_binary"; \
		"$$test_binary"; \
	done
	@bash ./test_artifact_checker.sh

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test_crc_utils: test_crc_utils.c $(APP_SRC)/rfid/crc_utils.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_circular_buffer: test_circular_buffer.c $(LF_UTILS)/circular_buffer.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_dataframe: test_dataframe.c $(APP_SRC)/utils/dataframe.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_hf_capture: test_hf_capture.c $(APP_SRC)/rfid/hf_capture.c | $(BUILD_DIR)
	$(CC) -DHF_CAPTURE_TEST -Icapture_stubs $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_active_slot_snapshot: test_active_slot_snapshot.c $(APP_SRC)/active_slot_snapshot_internal.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_active_slot_snapshot.c $(LDFLAGS) -o $@

$(BUILD_DIR)/test_device_settings_payload: \
		test_device_settings_payload.c \
		$(APP_SRC)/device_settings_payload_internal.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_device_settings_payload.c $(LDFLAGS) -o $@

$(BUILD_DIR)/test_keyboard_payload: test_keyboard_payload.c $(APP_SRC)/keyboard_payload.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) \
		-include stubs/ble_main.h \
		-include stubs/keyboard_hid.h \
		-include stubs/usb_main.h \
		$(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_hf_memory_safety: \
		test_hf_memory_safety.c \
		$(APP_SRC)/rfid/byte_mirror.c \
		$(APP_SRC)/rfid/nfctag/hf/nfc_14a_frame_internal.h \
		$(APP_SRC)/rfid/nfctag/hf/ntag_mirror_internal.h \
		$(HF_READER)/emv_trace_internal.h \
		$(HF_READER)/mf1_key_access_internal.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) test_hf_memory_safety.c \
		$(APP_SRC)/rfid/byte_mirror.c $(LDFLAGS) -o $@

$(BUILD_DIR)/test_iso_dep_reader: test_iso_dep_reader.c $(HF_READER)/iso_dep_reader.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_iso_dep_session: test_iso_dep_session.c $(HF_READER)/iso_dep_session.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_lf_helpers: \
		test_lf_helpers.c \
		$(LF_UTILS)/manchester.c \
		$(LF_UTILS)/diphase.c \
		$(LF_PROTOCOLS)/wiegand.c \
		$(LF_PROTOCOLS)/jablotron.c \
		$(APP_SRC)/rfid/parity.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_tag_persistence: \
		test_tag_persistence.c \
		$(APP_SRC)/rfid/nfctag/tag_emulation.c \
		$(APP_SRC)/rfid/crc_utils.c | $(BUILD_DIR)
	$(CC) -Istubs/tag_persistence $(CPPFLAGS) $(CFLAGS) -fshort-enums \
		$^ $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)
