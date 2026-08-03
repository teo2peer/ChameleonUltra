#ifndef DATA_CMD_H
#define DATA_CMD_H


// ******************************************************************
//                      CMD for device
//                  Range from 1000 -> 1999
// ******************************************************************
//
#define DATA_CMD_GET_APP_VERSION                (1000)
#define DATA_CMD_CHANGE_DEVICE_MODE             (1001)
#define DATA_CMD_GET_DEVICE_MODE                (1002)
#define DATA_CMD_SET_ACTIVE_SLOT                (1003)
#define DATA_CMD_SET_SLOT_TAG_TYPE              (1004)
#define DATA_CMD_SET_SLOT_DATA_DEFAULT          (1005)
#define DATA_CMD_SET_SLOT_ENABLE                (1006)
#define DATA_CMD_SET_SLOT_TAG_NICK              (1007)
#define DATA_CMD_GET_SLOT_TAG_NICK              (1008)
#define DATA_CMD_SLOT_DATA_CONFIG_SAVE          (1009)
#define DATA_CMD_ENTER_BOOTLOADER               (1010)
#define DATA_CMD_GET_DEVICE_CHIP_ID             (1011)
#define DATA_CMD_GET_DEVICE_ADDRESS             (1012)
#define DATA_CMD_SAVE_SETTINGS                  (1013)
#define DATA_CMD_RESET_SETTINGS                 (1014)
#define DATA_CMD_SET_ANIMATION_MODE             (1015)
#define DATA_CMD_GET_ANIMATION_MODE             (1016)
#define DATA_CMD_GET_GIT_VERSION                (1017)
#define DATA_CMD_GET_ACTIVE_SLOT                (1018)
#define DATA_CMD_GET_SLOT_INFO                  (1019)
#define DATA_CMD_WIPE_FDS                       (1020)
#define DATA_CMD_DELETE_SLOT_TAG_NICK           (1021)

#define DATA_CMD_GET_ENABLED_SLOTS              (1023)
#define DATA_CMD_DELETE_SLOT_SENSE_TYPE         (1024)
#define DATA_CMD_GET_BATTERY_INFO               (1025)
#define DATA_CMD_GET_BUTTON_PRESS_CONFIG        (1026)
#define DATA_CMD_SET_BUTTON_PRESS_CONFIG        (1027)
#define DATA_CMD_GET_LONG_BUTTON_PRESS_CONFIG   (1028)
#define DATA_CMD_SET_LONG_BUTTON_PRESS_CONFIG   (1029)
#define DATA_CMD_SET_BLE_PAIRING_KEY            (1030)
#define DATA_CMD_GET_BLE_PAIRING_KEY            (1031)
#define DATA_CMD_DELETE_ALL_BLE_BONDS           (1032)
#define DATA_CMD_GET_DEVICE_MODEL               (1033)
#define DATA_CMD_GET_DEVICE_SETTINGS            (1034)
#define DATA_CMD_GET_DEVICE_CAPABILITIES        (1035)
#define DATA_CMD_GET_BLE_PAIRING_ENABLE         (1036)
#define DATA_CMD_SET_BLE_PAIRING_ENABLE         (1037)
#define DATA_CMD_GET_ALL_SLOT_NICKS             (1038)
#define DATA_CMD_GET_SLEEP_TIMEOUT              (1039)
#define DATA_CMD_SET_SLEEP_TIMEOUT              (1040)
#define DATA_CMD_KEYBOARD_UPLOAD_BEGIN          (1041)
#define DATA_CMD_KEYBOARD_UPLOAD_CHUNK          (1042)
#define DATA_CMD_KEYBOARD_UPLOAD_COMMIT         (1043)
#define DATA_CMD_KEYBOARD_RUN                   (1044)
#define DATA_CMD_KEYBOARD_CANCEL                (1045)
#define DATA_CMD_KEYBOARD_GET_STATUS            (1046)
#define DATA_CMD_KEYBOARD_CLEAR                 (1047)
#define DATA_CMD_KEYBOARD_SET_TEMP_BLE_NAME     (1048)
#define DATA_CMD_KEYBOARD_ARM_BLE               (1049)
#define DATA_CMD_ACTIVE_SLOT_SNAPSHOT            (1050)
#define DATA_CMD_GET_KEYBOARD_HID_ENABLE        (1051)
#define DATA_CMD_SET_KEYBOARD_HID_ENABLE        (1052)
#define DATA_CMD_SET_RUNTIME_UNDERCOVER_MODE     (1053)

//
// ******************************************************************


// ******************************************************************
//                      CMD for hf reader
//                  Range from 2000 -> 2999
// ******************************************************************
//
#define DATA_CMD_HF14A_SCAN                     (2000)
#define DATA_CMD_MF1_DETECT_SUPPORT             (2001)
#define DATA_CMD_MF1_DETECT_PRNG                (2002)
#define DATA_CMD_MF1_STATIC_NESTED_ACQUIRE      (2003)
#define DATA_CMD_MF1_DARKSIDE_ACQUIRE           (2004)
#define DATA_CMD_MF1_DETECT_NT_DIST             (2005)
#define DATA_CMD_MF1_NESTED_ACQUIRE             (2006)
#define DATA_CMD_MF1_AUTH_ONE_KEY_BLOCK         (2007)
#define DATA_CMD_MF1_READ_ONE_BLOCK             (2008)
#define DATA_CMD_MF1_READ_BLOCKS                (2018)  /* auth once, read N consecutive blocks in a sector */
#define DATA_CMD_MF1_WRITE_ONE_BLOCK            (2009)
#define DATA_CMD_HF14A_RAW                      (2010)
#define DATA_CMD_HF14A_SCAN_KEEP                (2016)  /* scan+RATS, keep field alive for APDU exchange */
#define DATA_CMD_HF14A_AUTH_TRACE               (2017)  /* full anticoll + Crypto1 auth, every frame returned for inspection */
#define DATA_CMD_MF1_MANIPULATE_VALUE_BLOCK     (2011)
#define DATA_CMD_MF1_CHECK_KEYS_OF_SECTORS      (2012)
#define DATA_CMD_MF1_HARDNESTED_ACQUIRE         (2013)
#define DATA_CMD_MF1_ENC_NESTED_ACQUIRE         (2014)
#define DATA_CMD_MF1_CHECK_KEYS_ON_BLOCK        (2015)

#define DATA_CMD_HF14A_SET_FIELD_ON             (2100)
#define DATA_CMD_HF14A_SET_FIELD_OFF            (2101)

#define DATA_CMD_HF14A_GET_CONFIG               (2200)
#define DATA_CMD_HF14A_SET_CONFIG               (2201)
#define DATA_CMD_HF14A_SNIFF                    (2020)
#define DATA_CMD_HF_CAPTURE_START               (2021)
#define DATA_CMD_HF_CAPTURE_STATUS              (2022)
#define DATA_CMD_HF_CAPTURE_GET                 (2023)
#define DATA_CMD_HF_CAPTURE_STOP                (2024)
#define DATA_CMD_HF_CAPTURE_EVENT               (2025)  /* unsolicited notification; no request handler */

//
// ******************************************************************


// ******************************************************************
//                      CMD for lf reader
//                  Range from 3000 -> 3999
// ******************************************************************
//
#define DATA_CMD_EM410X_SCAN                    (3000)
#define DATA_CMD_EM410X_WRITE_TO_T55XX          (3001)
#define DATA_CMD_EM410X_ELECTRA_WRITE_TO_T55XX  (3006)
#define DATA_CMD_HIDPROX_SCAN                   (3002)
#define DATA_CMD_HIDPROX_WRITE_TO_T55XX         (3003)
#define DATA_CMD_PAC_SCAN                       (3014)
#define DATA_CMD_PAC_WRITE_TO_T55XX             (3015)
#define DATA_CMD_VIKING_SCAN                    (3004)
#define DATA_CMD_VIKING_WRITE_TO_T55XX          (3005)
#define DATA_CMD_ADC_GENERIC_READ               (3009)
#define DATA_CMD_IOPROX_SCAN                    (3010)
#define DATA_CMD_IOPROX_WRITE_TO_T55XX          (3011)
#define DATA_CMD_IOPROX_DECODE_RAW              (3012)
#define DATA_CMD_IOPROX_COMPOSE_ID              (3013)
#define DATA_CMD_LF_T55XX_WRITE                 (3016)
#define DATA_CMD_IDTECK_WRITE_TO_T55XX          (3018)
#define DATA_CMD_JABLOTRON_SCAN                 (3019)
#define DATA_CMD_JABLOTRON_WRITE_TO_T55XX       (3020)

//
// ******************************************************************


// ******************************************************************
//                      CMD for hf emulator
//                  Range from 4000 -> 4999
// ******************************************************************
//
#define DATA_CMD_MF1_WRITE_EMU_BLOCK_DATA       (4000)
#define DATA_CMD_HF14A_SET_ANTI_COLL_DATA       (4001)
#define DATA_CMD_MF1_SET_DETECTION_ENABLE       (4004)
#define DATA_CMD_MF1_GET_DETECTION_COUNT        (4005)
#define DATA_CMD_MF1_GET_DETECTION_LOG          (4006)
#define DATA_CMD_MF1_GET_DETECTION_ENABLE       (4007)
#define DATA_CMD_MF1_READ_EMU_BLOCK_DATA        (4008)
#define DATA_CMD_MF1_GET_EMULATOR_CONFIG        (4009)
#define DATA_CMD_MF1_GET_GEN1A_MODE             (4010)
#define DATA_CMD_MF1_SET_GEN1A_MODE             (4011)
#define DATA_CMD_MF1_GET_GEN2_MODE              (4012)
#define DATA_CMD_MF1_SET_GEN2_MODE              (4013)
#define DATA_CMD_MF1_GET_BLOCK_ANTI_COLL_MODE   (4014)
#define DATA_CMD_MF1_SET_BLOCK_ANTI_COLL_MODE   (4015)
#define DATA_CMD_MF1_GET_WRITE_MODE             (4016)
#define DATA_CMD_MF1_SET_WRITE_MODE             (4017)
#define DATA_CMD_HF14A_GET_ANTI_COLL_DATA       (4018)
#define DATA_CMD_MF0_NTAG_GET_UID_MAGIC_MODE    (4019)
#define DATA_CMD_MF0_NTAG_SET_UID_MAGIC_MODE    (4020)
#define DATA_CMD_MF0_NTAG_READ_EMU_PAGE_DATA    (4021)
#define DATA_CMD_MF0_NTAG_WRITE_EMU_PAGE_DATA   (4022)
#define DATA_CMD_MF0_NTAG_GET_VERSION_DATA      (4023)
#define DATA_CMD_MF0_NTAG_SET_VERSION_DATA      (4024)
#define DATA_CMD_MF0_NTAG_GET_SIGNATURE_DATA    (4025)
#define DATA_CMD_MF0_NTAG_SET_SIGNATURE_DATA    (4026)
#define DATA_CMD_MF0_NTAG_GET_COUNTER_DATA      (4027)
#define DATA_CMD_MF0_NTAG_SET_COUNTER_DATA      (4028)
#define DATA_CMD_MF0_NTAG_RESET_AUTH_CNT        (4029)
#define DATA_CMD_MF0_NTAG_GET_PAGE_COUNT        (4030)
#define DATA_CMD_MF0_NTAG_GET_WRITE_MODE        (4031)
#define DATA_CMD_MF0_NTAG_SET_WRITE_MODE        (4032)
#define DATA_CMD_MF0_NTAG_SET_DETECTION_ENABLE  (4033)
#define DATA_CMD_MF0_NTAG_GET_DETECTION_COUNT   (4034)
#define DATA_CMD_MF0_NTAG_GET_DETECTION_LOG     (4035)
#define DATA_CMD_MF0_NTAG_GET_DETECTION_ENABLE  (4036)
#define DATA_CMD_MF0_NTAG_GET_EMULATOR_CONFIG   (4037)
#define DATA_CMD_MF1_SET_FIELD_OFF_DO_RESET     (4038)
#define DATA_CMD_MF1_GET_FIELD_OFF_DO_RESET     (4039)
#define DATA_CMD_MF1_GET_PRNG_TYPE              (4040)  // 0=static 1=weak(LFSR) 2=hard(rand)
#define DATA_CMD_MF1_SET_PRNG_TYPE              (4041)
#define DATA_CMD_MF1_SET_RANDOM_UID_MODE        (4042)  // emulate a new random UID on each reader activation
#define DATA_CMD_MF1_GET_RANDOM_UID_MODE        (4043)
#define DATA_CMD_MF1_SET_READER_KEYS_ANIM       (4044)  // center-out rainbow LED animation for reader-key capture
#define DATA_CMD_MF1_READER_KEYS_RESELECT       (4045)  // bounded RF mute/reselect for automatic reader-key retries
#define DATA_CMD_SEOS_READ_EMU_DATA              (4046)
#define DATA_CMD_SEOS_WRITE_EMU_DATA             (4047)
#define DATA_CMD_SEOS_WRITE_EMU_KEYS             (4048)
//
// ******************************************************************


// ******************************************************************
//                      CMD for lf emulator
//                  Range from 5000 -> 5999
// ******************************************************************
//

//
// ******************************************************************
/* ISO14443-4 T=CL emulation commands */
#define DATA_CMD_HF14A_4_APDU_RECV              (6000)  /* non-blocking poll: firmware->host APDU */
#define DATA_CMD_HF14A_4_APDU_SEND              (6001)  /* host->firmware APDU response */
#define DATA_CMD_HF14A_4_SET_ANTI_COLL          (6002)  /* set UID/ATQA/SAK/ATS */
#define DATA_CMD_HF14A_4_STATIC_RESP            (6003)  /* add/clear static APDU response pair */
#define DATA_CMD_HF14A_4_READER_APDU            (6004)  /* select+RATS+send APDU, keep field   */
#define DATA_CMD_HF14A_4_EMV_SCAN               (6005)  /* full EMV scan in one call            */
#define DATA_CMD_HF14A_4_DESFIRE_SCAN           (6006)  /* DESFire enumerate in one call        */
#define DATA_CMD_HF14A_4_EMV_TRACE_START        (6007)  /* run versioned retained EMV trace     */
#define DATA_CMD_HF14A_4_EMV_TRACE_META         (6008)  /* get retained EMV trace metadata      */
#define DATA_CMD_HF14A_4_EMV_TRACE_GET          (6009)  /* page retained EMV trace records      */
#define DATA_CMD_HF14A_4_DEBUG_COUNTERS         (6010)  /* T=CL debug counters (diagnostics)    */
#define DATA_CMD_HF14A_4_READER_SESSION_START   (6011)  /* select/RATS and open reader session  */
#define DATA_CMD_HF14A_4_READER_SESSION_EXCHANGE (6012) /* exchange APDU in active session      */
#define DATA_CMD_HF14A_4_READER_SESSION_STOP    (6013)  /* deselect and close reader session    */
#define DATA_CMD_HF14A_4_READER_SESSION_START_APPLE_TRANSIT (6014) /* Apple Transit annotated START */

#define DATA_CMD_EM410X_SET_EMU_ID              (5000)
#define DATA_CMD_EM410X_GET_EMU_ID              (5001)
#define DATA_CMD_HIDPROX_SET_EMU_ID             (5002)
#define DATA_CMD_HIDPROX_GET_EMU_ID             (5003)
#define DATA_CMD_VIKING_SET_EMU_ID              (5004)
#define DATA_CMD_VIKING_GET_EMU_ID              (5005)
#define DATA_CMD_PAC_SET_EMU_ID                 (5006)
#define DATA_CMD_PAC_GET_EMU_ID                 (5007)
#define DATA_CMD_IOPROX_SET_EMU_ID              (5008)
#define DATA_CMD_IOPROX_GET_EMU_ID              (5009)
#define DATA_CMD_JABLOTRON_SET_EMU_ID           (5010)
#define DATA_CMD_JABLOTRON_GET_EMU_ID           (5011)
#define DATA_CMD_IDTECK_SET_EMU_ID              (5012)
#define DATA_CMD_IDTECK_GET_EMU_ID              (5013)

#define DATA_CMD_EM4X05_SCAN                    (3030)
#define DATA_CMD_EM4X05_READSNIFF               (3032)
#define DATA_CMD_LF_SNIFF                       (3031)


// ******************************************************************
//                      CMD for BLE (2.4GHz radio)
//                  Range from 7000 -> 7999
// ******************************************************************
//
// NOTE: passive scanner is LISTEN-ONLY when started with active=0 (no scan
// requests transmitted). All other 7000-block commands in this fork can be
// used at any scope the operator picks — single-target, scan-buffer-wide, or
// environment-wide broadcast — per the CLAUDE.md fork-specific exemption.
#define DATA_CMD_BLE_SCAN_START                 (7000)  /* start passive BLE scan (listen-only) */
#define DATA_CMD_BLE_SCAN_STOP                  (7001)  /* stop passive BLE scan                */
#define DATA_CMD_BLE_SCAN_GET_COUNT             (7002)  /* number of distinct devices seen      */
#define DATA_CMD_BLE_SCAN_GET_RESULTS           (7003)  /* paged device records by start index  */

#define DATA_CMD_BLE_ADVERTISING_SET            (7004)  /* enable/disable local advertising     */
#define DATA_CMD_BLE_ADVERTISING_GET            (7005)  /* query local advertising state        */
#define DATA_CMD_BLE_LINK_PROBE                 (7006)  /* probe target[0] or batch-scan all[1] */

// GATT fuzzing harness — connects (central role), enumerates GATT
// characteristics and writes mutated payloads to a chosen characteristic.
// Per-call scope selectable: single target, scan-buffer-wide (every address
// in the passive scanner's buffer), or environment-wide broadcast.
// DATA_CMD_BLE_DISCONNECT frees the current link so the target can
// reconnect to its normal source.
#define DATA_CMD_BLE_CONNECT                    (7010)  /* connect to target: addr_type[1]|addr[6] */
#define DATA_CMD_BLE_DISCONNECT                 (7011)  /* disconnect / free the target            */
#define DATA_CMD_BLE_CENTRAL_STATE              (7012)  /* poll connection/discovery/fuzz state    */
#define DATA_CMD_BLE_GATT_DISCOVER              (7013)  /* enumerate target GATT characteristics   */
#define DATA_CMD_BLE_GATT_GET_CHARS             (7014)  /* paged characteristic list               */
#define DATA_CMD_BLE_FUZZ_START                 (7015)  /* value_handle[2]|max_iter[2]|interval[2] */
#define DATA_CMD_BLE_FUZZ_STOP                  (7016)  /* stop fuzzing                            */
#define DATA_CMD_BLE_FUZZ_GET_LOG               (7017)  /* paged fuzz log by start index           */
#define DATA_CMD_BLE_GATT_READ                  (7018)  /* read a characteristic value: handle[2] */
#define DATA_CMD_BLE_GATT_GET_READ              (7019)  /* fetch last GATT read result            */
#define DATA_CMD_BLE_SUBSCRIBE                   (7020)  /* CCCD subscribe: cccd_handle[2]|mode[1] */
#define DATA_CMD_BLE_GET_NOTIFICATIONS          (7021)  /* paged received notifications           */
#define DATA_CMD_BLE_FIND_CCCD                   (7022)  /* discover a char's CCCD: value_handle[2] */
#define DATA_CMD_BLE_GET_CCCD                    (7023)  /* fetch CCCD lookup result               */
#define DATA_CMD_BLE_GATT_WRITE                  (7024)  /* write a characteristic: handle[2]|data */
#define DATA_CMD_BLE_GET_WRITE                   (7025)  /* fetch last GATT write result           */
#define DATA_CMD_BLE_GET_MTU                     (7026)  /* effective ATT MTU of the target link   */
#define DATA_CMD_BLE_DESC_DISCOVER               (7027)  /* enumerate all descriptors of target    */
#define DATA_CMD_BLE_DESC_GET                     (7028)  /* paged descriptor list                  */
#define DATA_CMD_BLE_SVC_DISCOVER                (7029)  /* discover primary services              */
#define DATA_CMD_BLE_SVC_GET                     (7030)  /* paged primary-service list             */
#define DATA_CMD_BLE_DEVICE_INFO                 (7031)  /* read target GAP/DIS/battery info (RO)  */
#define DATA_CMD_BLE_GET_DEVICE_INFO             (7032)  /* fetch collected device-info fields     */

// Own-radio identity / radio power (cybersecurity fork additions).
// Identity always mutates OUR OWN radio. Radio power is local too. The
// environment-wide-broadcast tools live in the next block.
#define DATA_CMD_BLE_SET_ADDR                   (7040)  /* mode[1]|(addr[6]) — 0 restore / 1 static / 2 RPA / 3 NRPA */
#define DATA_CMD_BLE_GET_ADDR                   (7041)  /* out: addr_type[1] | addr[6 LE]                              */
#define DATA_CMD_BLE_RADIO_SET                  (7042)  /* on[1] — 0 silent (stop adv+scan+drop central), 1 resume      */
#define DATA_CMD_BLE_RADIO_GET                  (7043)  /* out: radio_on[1] | adv[1] | scan[1] | central_link[1]        */

// Environment-wide stress / broadcast tools (cybersecurity fork, see CLAUDE.md
// fork-specific exemption — operator-authorised lab use). Every command in
// this block accepts a `scope` selector so the operator chooses per call:
//   scope 0 = single target (already-connected central link, or host-picked addr)
//   scope 1 = scan-buffer-wide (every address the passive scanner has cached)
//   scope 2 = full environment-wide broadcast — spam the 2.4 GHz BLE spectrum
//             (non-connectable advertising with maximum payload, minimum
//             regulatory interval) so every scanner / peer in range sees it
#define DATA_CMD_BLE_FLOOD_START                (7044)  /* scope[1]|handle[2]|size[1]|max_iter[2]|interval_ms[2] */
#define DATA_CMD_BLE_FLOOD_STOP                 (7045)  /* stop flood                                              */
#define DATA_CMD_BLE_FLOOD_COUNT                (7046)  /* out: sent[4 BE] — accepted WRITE_CMDs                    */
#define DATA_CMD_BLE_KICK                       (7047)  /* scope[1]|cycles[1] — 1..10 disconnect cycles             */
#define DATA_CMD_BLE_ADV_FLOOD_START            (7050)  /* scope[1]|fill_byte[1]|interval_units[1] — 2.4 GHz broadcast */
#define DATA_CMD_BLE_ADV_FLOOD_STOP             (7051)  /* stop environment-wide broadcast spam                      */

// Advertising lab. These commands create validated legacy AD structures
// supplied by the host; no profile can force a peer to connect.
#define DATA_CMD_BLE_ADV_LAB_START              (7052)  /* start vendor-neutral custom/raw/rotating advertisement     */
#define DATA_CMD_BLE_ADV_LAB_STATUS             (7053)  /* read advertising-lab state                                 */
#define DATA_CMD_BLE_ADV_LAB_STOP               (7054)  /* stop lab advertisement and restore normal advertising      */

#endif
