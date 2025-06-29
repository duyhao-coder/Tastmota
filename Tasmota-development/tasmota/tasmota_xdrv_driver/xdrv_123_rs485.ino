/*********************************************************************************************\
 * RS485 Driver for Tasmota (XDRV_123)
\*********************************************************************************************/

#ifdef USE_RS485

#include "TasmotaModbus.h"

#define XDRV_123 123
#define RS485_MODBUS_SPEED 9600

bool rs485_active = false;
TasmotaModbus *RS485Modbus = nullptr;

// Lưu lại chân RX/TX do người dùng chọn trên web
uint8_t rs485_rx_pin = 0;
uint8_t rs485_tx_pin = 0;

void RS485DetectGpio(void) {
  rs485_rx_pin = 0;
  rs485_tx_pin = 0;
  for (uint32_t i = 0; i < MAX_GPIO_PIN; i++) {
    if (Settings->gpio_pin[i] == GPIO_RS485_RX) {
      rs485_rx_pin = GpioPin(i);
    }
    if (Settings->gpio_pin[i] == GPIO_RS485_TX) {
      rs485_tx_pin = GpioPin(i);
    }
  }
}

void RS485Init(void) {
  RS485DetectGpio();
  rs485_active = false;
  if (rs485_rx_pin && rs485_tx_pin) {
    RS485Modbus = new TasmotaModbus(rs485_rx_pin, rs485_tx_pin);
    uint8_t result = RS485Modbus->Begin(RS485_MODBUS_SPEED);
    if (result) {
      if (2 == result) {
        ClaimSerial();
      }
      rs485_active = true;
    }
  }
}

void RS485ShowWeb(void) {
  WSContentSend_PD(PSTR("{s}RS485 Status{m}%s{e}"), rs485_active ? "Active" : "Inactive");
  WSContentSend_PD(PSTR("{s}RS485 RX Pin{m}%u{e}"), rs485_rx_pin);
  WSContentSend_PD(PSTR("{s}RS485 TX Pin{m}%u{e}"), rs485_tx_pin);
}

void RS485ShowJSON(void) {
  ResponseAppend_P(PSTR(",\"RS485\":{\"Status\":\"%s\",\"RX\":%u,\"TX\":%u}"),
    rs485_active ? "Active" : "Inactive", rs485_rx_pin, rs485_tx_pin);
}

bool Xdrv123(uint32_t function) {
  switch (function) {
    case FUNC_INIT:
      RS485Init();
      break;
    case FUNC_WEB_SENSOR:
      RS485ShowWeb();
      break;
    case FUNC_JSON_APPEND:
      RS485ShowJSON();
      break;
    default:
      break;
  }
  return true;
}

#endif  // USE_RS485