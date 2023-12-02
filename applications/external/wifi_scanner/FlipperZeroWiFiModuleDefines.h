#define WIFI_MODULE_INIT_VERSION "WFSM_0.1"

#define MODULE_CONTEXT_INITIALIZATION WIFI_MODULE_INIT_VERSION
#define MODULE_CONTEXT_MONITOR "monitor"
#define MODULE_CONTEXT_SCAN "scan"
#define MODULE_CONTEXT_SCAN_ANIMATION "scan_anim"
#define MODULE_CONTEXT_MONITOR_ANIMATION "monitor_anim"

#define MODULE_CONTROL_COMMAND_NEXT 'n'
#define MODULE_CONTROL_COMMAND_PREVIOUS 'p'
#define MODULE_CONTROL_COMMAND_SCAN 's'
#define MODULE_CONTROL_COMMAND_MONITOR 'm'
#define MODULE_CONTROL_COMMAND_RESTART 'r'

#define FLIPPERZERO_SERIAL_BAUD 115200

#define NA 0

#include <cfw/cfw.h>

#define UART_CH \
    (CFW_SETTINGS()->uart_esp_channel == UARTDefault ? FuriHalUartIdUSART1 : FuriHalUartIdLPUART1)