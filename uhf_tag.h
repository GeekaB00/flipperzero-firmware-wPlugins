#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Reserved Memory Bank
typedef struct {
    uint8_t kill_password[2]; // 2 bytes (16 bits) for kill password
    uint8_t access_password[2]; // 2 bytes (16 bits) for access password
} ReservedMemoryBank;

// EPC Memory Bank
typedef struct {
    size_t size; // Size of EPC memory data
    uint8_t data[18]; // 2 bytes for CRC16, 2 bytes for PC, and max 14 bytes for EPC
} EPCMemoryBank;

// TID Memory Bank
typedef struct {
    size_t size; // Size of TID memory data
    uint8_t data[16]; // 4 bytes for Class ID and max 12 bytes for TID data
} TIDMemoryBank;

// User Memory Bank
typedef struct {
    size_t size; // Size of user memory data
    uint8_t data[64]; // Assuming max 512 bits (64 bytes) for User Memory
} UserMemoryBank;

// EPC Gen 2 Tag containing all memory banks
typedef struct {
    ReservedMemoryBank reserved;
    EPCMemoryBank epc;
    TIDMemoryBank tid;
    UserMemoryBank user;
} UHFTag;

UHFTag* uhf_tag_alloc();
void uhf_tag_free(UHFTag* uhf_tag);

void uhf_tag_set_kill_pwd(UHFTag* uhf_tag, uint8_t* data_in);
void uhf_tag_set_access_pwd(UHFTag* uhf_tag, uint8_t* data_in);
void uhf_tag_set_epc(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
void uhf_tag_set_tid(UHFTag* uhf_tag, uint8_t* data_in, size_t size);
void uhf_tag_set_user(UHFTag* uhf_tag, uint8_t* data_in, size_t size);

uint8_t* uhf_tag_get_kill_pwd(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_access_pwd(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_epc(UHFTag* uhf_tag);
size_t uhf_tag_get_epc_size(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_tid(UHFTag* uhf_tag);
size_t uhf_tag_get_tid_size(UHFTag* uhf_tag);
uint8_t* uhf_tag_get_user(UHFTag* uhf_tag);
size_t uhf_tag_get_user_size(UHFTag* uhf_tag);
