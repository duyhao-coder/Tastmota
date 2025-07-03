#ifdef USE_RS485
#ifdef USE_O3_GAS_SENSOR

#define XSNS_124 124  // ID module tùy chỉnh
#define XRS485_30 30

struct O3Sensor_t
{
    bool valid = false;
    uint16_t concentration = 0;
    char name[5] = "O3";
} O3Sensor;

#define O3_ADDRESS_ID 0x22
#define O3_ADDRESS_VALUE 0x0006  // Thanh ghi chứa giá trị O3
#define O3_FUNCTION_CODE 0x03
#define O3_ADDRESS_CHECK 0x0100    // Dùng kiểm tra kết nối (Equipment Address)

bool O3isConnected()
{
    if (!RS485.active)
        return false;
//

    RS485.Rs485Modbus->Send(O3_ADDRESS_ID, O3_FUNCTION_CODE, O3_ADDRESS_CHECK, 0x01);
    delay(200);
    RS485.Rs485Modbus->ReceiveReady();

    uint8_t buffer[8];
    uint8_t error = RS485.Rs485Modbus->ReceiveBuffer(buffer, 8);

    if (error)
    {
        AddLog(LOG_LEVEL_INFO, PSTR("O3 sensor error %d"), error);
        return false;
    }
    else
    {
        uint16_t check_val = (buffer[3] << 8) | buffer[4];
        if (check_val <= 255)  // Equipment Address là 0~255
            return true;
    }
    return false;
}
void O3Init()
{
    if (!RS485.active)
        return;

    O3Sensor.valid = O3isConnected();

    if (O3Sensor.valid)
        Rs485SetActiveFound(O3_ADDRESS_ID, O3Sensor.name);

    if (O3Sensor.valid)
        AddLog(LOG_LEVEL_INFO, PSTR("O3 sensor is connected"));
    else
        AddLog(LOG_LEVEL_INFO, PSTR("O3 sensor is not detected"));
}


const char HTTP_SNS_O3[] PROGMEM = "{s}O₃ (ppm){m} %.1f";
#define D_JSON_O3 "O3"

void O3ReadData()
{
    if (!O3Sensor.valid)
        return;

    if (isWaitingResponse(O3_ADDRESS_ID))
        return;

    if ((RS485.requestSent[O3_ADDRESS_ID] == 0) && (RS485.lastRequestTime == 0))
    {
        RS485.Rs485Modbus->Send(O3_ADDRESS_ID, O3_FUNCTION_CODE, O3_ADDRESS_VALUE, 1);
        RS485.requestSent[O3_ADDRESS_ID] = 1;
        RS485.lastRequestTime = millis();
    }

    if ((RS485.requestSent[O3_ADDRESS_ID] == 1) && (millis() - RS485.lastRequestTime >= 200))
    {
        if (!RS485.Rs485Modbus->ReceiveReady())
            return;

        uint8_t buffer[8];
        uint8_t error = RS485.Rs485Modbus->ReceiveBuffer(buffer, 8);

        if (error)
        {
            AddLog(LOG_LEVEL_INFO, PSTR("Modbus O3 error: %d"), error);
        }
        else
        {
            uint16_t raw_ppm = (buffer[3] << 8) | buffer[4];
            O3Sensor.concentration = raw_ppm;
        }

        RS485.requestSent[O3_ADDRESS_ID] = 0;
        RS485.lastRequestTime = 0;
    }
}

void O3Show(bool json)
{
    if (json)
    {
        ResponseAppend_P(PSTR(",\"%s\":{"), O3Sensor.name);
        ResponseAppend_P(PSTR("\"" D_JSON_O3 "\":%.1f"), O3Sensor.concentration / 10.0);  // nếu dữ liệu nhân 10
        ResponseJsonEnd();
    }
#ifdef USE_WEBSERVER
    else
    {
        WSContentSend_PD(HTTP_SNS_O3, O3Sensor.concentration / 10.0);
    }
#endif
}

bool Xsns124(uint32_t function)
{
    if (!Rs485Enabled(XRS485_30))
        return false;

    bool result = false;

    if (FUNC_INIT == function)
    {
        O3Init();
    }
    else if (O3Sensor.valid)
    {
        switch (function)
        {
        case FUNC_EVERY_250_MSECOND:
            O3ReadData();
            break;
        case FUNC_JSON_APPEND:
            O3Show(true);
            break;
#ifdef USE_WEBSERVER
        case FUNC_WEB_SENSOR:
            O3Show(false);
            break;
#endif
        }
    }
    return result;
}
#endif
#endif
