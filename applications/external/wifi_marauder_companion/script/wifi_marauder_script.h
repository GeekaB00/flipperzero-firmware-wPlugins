/*
 * ----------------------------------------------------------------------------------------------------
 * STEPS TO ADD A NEW STAGE:
 * 
 * wifi_marauder_script.h
 * - Complement WifiMarauderScriptStageType enum with new stage
 * - Create struct WifiMarauderScriptStage???? for the new stage
 * 
 * wifi_marauder_script.c
 * - Create function "WifiMarauderScriptStage????* _wifi_marauder_script_get_stage_????(cJSON *stages)"
 * - Change _wifi_marauder_script_load_stages() to load new stage
 * - Add case to free memory in wifi_marauder_script_free()
 * 
 * wifi_marauder_script_executor.c
 * - Create function "void _wifi_marauder_script_execute_????(WifiMarauderScriptStage????* stage)"
 * - Add case in wifi_marauder_script_execute_stage()
 * 
 * ----------------------------------------------------------------------------------------------------
 * IMPLEMENTED STAGES (In order of execution):
 * - Scan
 * - Select
 * - Deauth
 * - Probe
 * - Sniff raw
 * - Sniff beacon
 * - Sniff deauth
 * - Sniff Espressif
 * - Sniff PMKID
 * - Sniff Pwnagotchi
 * - Beacon List
 * - Beacon Random
 * - Beacon Ap
 * ----------------------------------------------------------------------------------------------------
 * SCRIPT SYNTAX:
 * {
 *     "meta": {
 *         "description": "My script",
 *         "repeat": times the script will repeat (default 1),
 *         "enableLed": true (default) | false,
 *         "savePcap": true (default) | false
 *     },
 *     "stages": {
 *         "scan": {
 *             "type": "ap" | "station",
 *             "timeout": seconds,
 *             "channel": 1-11
 *         },
 *         "select": {
 *             "type": "ap" | "station" | "ssid",
 *             "filter": "all" | "contains -f '{SSID fragment}' or equals '{SSID}' or ...",
 *             "index": index number,
 *             "indexes": [0, 1, 2, 3...],
 *         },
 *         "deauth": {
 *             "timeout": seconds
 *         },
 *         "probe": {
 *             "timeout": seconds
 *         },
 *         "sniffRaw": {
 *             "timeout": seconds
 *         },
 *         "sniffBeacon": {
 *             "timeout": seconds
 *         },
 *         "sniffDeauth": {
 *             "timeout": seconds
 *         },
 *         "sniffEsp": {
 *             "timeout": seconds
 *         },
 *         "sniffPmkid": {
 *             "forceDeauth": true (default) | false,
 *             "channel": 1-11,
 *             "timeout": seconds
 *         },
 *         "sniffPwn": {
 *             "timeout": seconds
 *         },
 *         "beaconList": {
 *             "ssids": [
 *                 "SSID 1",
 *                 "SSID 2",
 *                 "SSID 3"
 *             ],
 *             "generate": number of random SSIDs that will be generated,
 *             "timeout": seconds
 *         }
 *         "beaconAp": {
 *             "timeout": seconds
 *         }
 *     }
 * }
 * 
 * Note: It is possible to inform "stages" as an array, allowing ordering and repetition of stages of the same type:
 *     "stages": [
 *       {
 *         "beaconList": { "ssids": ["SSID 1", "SSID 2"] }
 *       },
 *       {
 *         "beaconList": { "generate": 4 }
 *       },
 *     ]
 * ----------------------------------------------------------------------------------------------------
 */

#pragma once

#include <storage/storage.h>
#include "cJSON.h"

typedef enum {
    WifiMarauderScriptBooleanFalse = 0,
    WifiMarauderScriptBooleanTrue = 1,
    WifiMarauderScriptBooleanUndefined = 2
} WifiMarauderScriptBoolean;

typedef enum {
    WifiMarauderScriptStageTypeScan,
    WifiMarauderScriptStageTypeSelect,
    WifiMarauderScriptStageTypeDeauth,
    WifiMarauderScriptStageTypeProbe,
    WifiMarauderScriptStageTypeSniffRaw,
    WifiMarauderScriptStageTypeSniffBeacon,
    WifiMarauderScriptStageTypeSniffDeauth,
    WifiMarauderScriptStageTypeSniffEsp,
    WifiMarauderScriptStageTypeSniffPmkid,
    WifiMarauderScriptStageTypeSniffPwn,
    WifiMarauderScriptStageTypeBeaconList,
    WifiMarauderScriptStageTypeBeaconAp,
} WifiMarauderScriptStageType;

typedef enum {
    WifiMarauderScriptScanTypeAp,
    WifiMarauderScriptScanTypeStation
} WifiMarauderScriptScanType;

typedef enum {
    WifiMarauderScriptSelectTypeAp,
    WifiMarauderScriptSelectTypeStation,
    WifiMarauderScriptSelectTypeSsid
} WifiMarauderScriptSelectType;

// Stages
typedef struct WifiMarauderScriptStage {
    WifiMarauderScriptStageType type;
    void* stage;
    struct WifiMarauderScriptStage* next_stage;
} WifiMarauderScriptStage;

typedef struct WifiMarauderScriptStageScan {
    WifiMarauderScriptScanType type;
    int channel;
    int timeout;
} WifiMarauderScriptStageScan;

typedef struct WifiMarauderScriptStageSelect {
    WifiMarauderScriptSelectType type;
    char* filter;
    int* indexes;
    // TODO: Implement a feature to not select the same items in the next iteration of the script
    bool allow_repeat;
} WifiMarauderScriptStageSelect;

typedef struct WifiMarauderScriptStageDeauth {
    int timeout;
} WifiMarauderScriptStageDeauth;

typedef struct WifiMarauderScriptStageProbe {
    int timeout;
} WifiMarauderScriptStageProbe;

typedef struct WifiMarauderScriptStageSniffRaw {
    int timeout;
} WifiMarauderScriptStageSniffRaw;

typedef struct WifiMarauderScriptStageSniffBeacon {
    int timeout;
} WifiMarauderScriptStageSniffBeacon;

typedef struct WifiMarauderScriptStageSniffDeauth {
    int timeout;
} WifiMarauderScriptStageSniffDeauth;

typedef struct WifiMarauderScriptStageSniffEsp {
    int timeout;
} WifiMarauderScriptStageSniffEsp;

typedef struct WifiMarauderScriptStageSniffPmkid {
    bool force_deauth;
    int channel;
    int timeout;
} WifiMarauderScriptStageSniffPmkid;

typedef struct WifiMarauderScriptStageSniffPwn {
    int timeout;
} WifiMarauderScriptStageSniffPwn;

typedef struct WifiMarauderScriptStageBeaconList {
    char** ssids;
    int ssid_count;
    int random_ssids;
    int timeout;
} WifiMarauderScriptStageBeaconList;

typedef struct WifiMarauderScriptStageBeaconAp {
    int timeout;
} WifiMarauderScriptStageBeaconAp;

// Script
typedef struct WifiMarauderScript {
    char* name;
    char* description;
    WifiMarauderScriptStage* first_stage;
    WifiMarauderScriptBoolean enable_led;
    WifiMarauderScriptBoolean save_pcap;
    int repeat;
} WifiMarauderScript;

WifiMarauderScript* wifi_marauder_script_alloc();
WifiMarauderScript* wifi_marauder_script_parse_raw(const char* script_raw);
WifiMarauderScript* wifi_marauder_script_parse_file(const char* file_path, Storage* storage);
WifiMarauderScriptStage* wifi_marauder_script_get_stage(
    WifiMarauderScript* script,
    WifiMarauderScriptStageType stage_type);
WifiMarauderScriptStage* wifi_marauder_script_get_last_stage(WifiMarauderScript* script);
void wifi_marauder_script_free(WifiMarauderScript* script);
