#include "mifare_common.h"

MifareType mifare_common_get_type(uint8_t ATQA0, uint8_t ATQA1, uint8_t SAK) {
    MifareType type = MifareTypeUnknown;

    if((ATQA0 == 0x44) && (ATQA1 == 0x00) && (SAK == 0x00)) {
        type = MifareTypeUltralight;
    }

    return type;
}
