#include "furi_hal_nfc_i.h"
#include "furi_hal_nfc_tech_i.h"

// Prevent FDT timer from starting
#define FURI_HAL_NFC_FELICA_LISTENER_FDT_COMP_FC (INT32_MAX)

static FuriHalNfcError furi_hal_nfc_felica_poller_init(FuriHalSpiBusHandle* handle) {
    // Enable Felica mode, AM modulation
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_MODE,
        ST25R3916_REG_MODE_om_mask | ST25R3916_REG_MODE_tr_am,
        ST25R3916_REG_MODE_om_felica | ST25R3916_REG_MODE_tr_am_am);

    // 10% ASK modulation
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_TX_DRIVER,
        ST25R3916_REG_TX_DRIVER_am_mod_mask,
        ST25R3916_REG_TX_DRIVER_am_mod_10percent);

    // Use regulator AM, resistive AM disabled
    st25r3916_clear_reg_bits(
        handle,
        ST25R3916_REG_AUX_MOD,
        ST25R3916_REG_AUX_MOD_dis_reg_am | ST25R3916_REG_AUX_MOD_res_am);

    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_BIT_RATE,
        ST25R3916_REG_BIT_RATE_txrate_mask | ST25R3916_REG_BIT_RATE_rxrate_mask,
        ST25R3916_REG_BIT_RATE_txrate_212 | ST25R3916_REG_BIT_RATE_rxrate_212);

    // Receive configuration
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_RX_CONF1,
        ST25R3916_REG_RX_CONF1_lp0 | ST25R3916_REG_RX_CONF1_hz_12_80khz);

    // Correlator setup
    st25r3916_write_reg(
        handle,
        ST25R3916_REG_CORR_CONF1,
        ST25R3916_REG_CORR_CONF1_corr_s6 | ST25R3916_REG_CORR_CONF1_corr_s4 |
            ST25R3916_REG_CORR_CONF1_corr_s3);

    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_felica_poller_deinit(FuriHalSpiBusHandle* handle) {
    UNUSED(handle);

    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_felica_listener_init(FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    // Enable Felica mode, AM modulation
    st25r3916_change_reg_bits(
        handle,
        ST25R3916_REG_MODE,
        ST25R3916_REG_MODE_om_mask | ST25R3916_REG_MODE_tr_am,
        ST25R3916_REG_MODE_om_felica | ST25R3916_REG_MODE_tr_am_am);

    return FuriHalNfcErrorNone;
}

static FuriHalNfcError furi_hal_nfc_felica_listener_deinit(FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

static FuriHalNfcEvent furi_hal_nfc_felica_listener_wait_event(uint32_t timeout_ms) {
    UNUSED(timeout_ms);
    FuriHalNfcEvent event = furi_hal_nfc_wait_event_common(timeout_ms);

    return event;
}

FuriHalNfcError furi_hal_nfc_felica_listener_tx(
    FuriHalSpiBusHandle* handle,
    const uint8_t* tx_data,
    size_t tx_bits) {
    UNUSED(handle);
    UNUSED(tx_data);
    UNUSED(tx_bits);
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_felica_listener_sleep(FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

FuriHalNfcError furi_hal_nfc_felica_listener_idle(FuriHalSpiBusHandle* handle) {
    UNUSED(handle);
    return FuriHalNfcErrorNone;
}

const FuriHalNfcTechBase furi_hal_nfc_felica = {
    .poller =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_POLLER_FDT_COMP_FC,
                    .fwt = FURI_HAL_NFC_POLLER_FWT_COMP_FC,
                },
            .init = furi_hal_nfc_felica_poller_init,
            .deinit = furi_hal_nfc_felica_poller_deinit,
            .wait_event = furi_hal_nfc_wait_event_common,
            .tx = furi_hal_nfc_poller_tx_common,
            .rx = furi_hal_nfc_common_fifo_rx,
        },

    .listener =
        {
            .compensation =
                {
                    .fdt = FURI_HAL_NFC_FELICA_LISTENER_FDT_COMP_FC,
                },
            .init = furi_hal_nfc_felica_listener_init,
            .deinit = furi_hal_nfc_felica_listener_deinit,
            .wait_event = furi_hal_nfc_felica_listener_wait_event,
            .tx = furi_hal_nfc_felica_listener_tx,
            .rx = furi_hal_nfc_common_fifo_rx,
            .sleep = furi_hal_nfc_felica_listener_sleep,
            .idle = furi_hal_nfc_felica_listener_idle,
        },
};
