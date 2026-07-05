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
#define DATA_CMD_GENERIC_READ                   (3007)
#define DATA_CMD_CORR_GENERIC_READ              (3008)
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
// NOTE: these are LISTEN-ONLY. The passive scanner uses the SoftDevice
// observer role with active=0, so the device never transmits (no scan
// requests, no advertisements, no carrier). It only receives advertising
// packets already broadcast by nearby devices.
#define DATA_CMD_BLE_SCAN_START                 (7000)  /* start passive BLE scan (listen-only) */
#define DATA_CMD_BLE_SCAN_STOP                  (7001)  /* stop passive BLE scan                */
#define DATA_CMD_BLE_SCAN_GET_COUNT             (7002)  /* number of distinct devices seen      */
#define DATA_CMD_BLE_SCAN_GET_RESULTS           (7003)  /* paged device records by start index  */

#define DATA_CMD_BLE_ADVERTISING_SET            (7004)  /* enable/disable local advertising     */
#define DATA_CMD_BLE_ADVERTISING_GET            (7005)  /* query local advertising state        */
#define DATA_CMD_BLE_LINK_PROBE                 (7006)  /* probe target[0] or batch-scan all[1] */

// Directed GATT fuzzing harness — point-to-point against ONE user-specified
// target address. Connects out (central role), enumerates the target's GATT
// characteristics and writes mutated payloads to a chosen characteristic. It
// NEVER broadcasts to the environment; everything is scoped to the single
// connected target, and DATA_CMD_BLE_DISCONNECT frees it to reconnect normally.
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

#endif
