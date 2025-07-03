#ifdef USE_RS485
#ifdef USE_XX_XX

#define XSNS_123 123
#define XRS485_31 31

struct EP-SO2-01t
{
    bool valid = false;
    uint16_t temperature = 0.0;
    char name[5] = "EP-SO2-01";
} EP-SO2-01;

#define EP-SO2-01_ADDRESS_ID 0x21
#define EP-SO2-01_ADDRESS_VALUE 0x0006
#define EP-SO2-01_FUNCTION_CODE 0x03
#define EP-SO2-01_ADDRESS_CHECK 0x0100

bool EP-SO2-01isConnected()
{
    if (!RS485.active)
        return false;

    RS485.Rs485Modbus->Send(EP-SO2-01_ADDRESS_ID, EP-SO2-01_FUNCTION_CODE, EP-SO2-01_ADDRESS_CHECK, 0x01);

    delay(200);

    RS485.Rs485Modbus->ReceiveReady();

    uint8_t buffer[8];
    uint8_t error = RS485.Rs485Modbus->ReceiveBuffer(buffer, 8);

    if (error)
    {
        AddLog(LOG_LEVEL_INFO, PSTR("EP-SO2-01 has error %d"), error);
        return false;
    }
    else
    {
        uint16_t check_EP-SO2-01 = (buffer[3] << 8) | buffer[4];
        if (check_EP-SO2-01 == EP-SO2-01_ADDRESS_ID)
            return true;
    }
    return false;
}

void EP-SO2-01Init()
{
    if (!RS485.active)
        return;
    EP-SO2-01.valid = EP-SO2-01isConnected();
    if (EP-SO2-01.valid)
        Rs485SetActiveFound(EP-SO2-01_ADDRESS_ID, EP-SO2-01.name);
    AddLog(LOG_LEVEL_INFO, PSTR(EP-SO2-01.valid ? "EP-SO2-01 is connected" : "EP-SO2-01 is not detected"));
}

const char HTTP_SNS_EP-SO2-01[] PROGMEM = "{s} Temperature {m} %.1f";
#define D_JSON_EP-SO2-01 "EP-SO2-01"

void EP-SO2-01ReadData()
{
    if (!EP-SO2-01.valid)
        return;

    if (isWaitingResponse(EP-SO2-01_ADDRESS_ID))
        return;

    if ((RS485.requestSent[EP-SO2-01_ADDRESS_ID] == 0) && (RS485.lastRequestTime == 0))
    {
        RS485.Rs485Modbus->Send(EP-SO2-01_ADDRESS_ID, EP-SO2-01_FUNCTION_CODE, EP-SO2-01_ADDRESS_VALUE, 1);
        RS485.requestSent[EP-SO2-01_ADDRESS_ID] = 1;
        RS485.lastRequestTime = millis();
    }

    if ((RS485.requestSent[EP-SO2-01_ADDRESS_ID] == 1) && (millis() - RS485.lastRequestTime >= 200))
    {
        if (!RS485.Rs485Modbus->ReceiveReady())
            return;
        uint8_t buffer[8];
        uint8_t error = RS485.Rs485Modbus->ReceiveBuffer(buffer, 8);

        if (error)
        {
            AddLog(LOG_LEVEL_INFO, PSTR("Modbus EP-SO2-01 error: %d"), error);
        }
        else
        {
            uint16_t temperatureRaw = (buffer[3] << 8) | buffer[4];
            EP-SO2-01.temperature = temperatureRaw;
        }
        RS485.requestSent[EP-SO2-01_ADDRESS_ID] = 0;
        RS485.lastRequestTime = 0;
    }
}

void EP-SO2-01Show(bool json)
{
    if (json)
    {
        ResponseAppend_P(PSTR(",\"%s\":{"), EP-SO2-01.name);
        ResponseAppend_P(PSTR("\"" D_JSON_EP-SO2-01 "\":%.1f"), EP-SO2-01.temperature);
        ResponseJsonEnd();
    }
#ifdef USE_WEBSERVER
    else
    {
        WSContentSend_PD(HTTP_SNS_EP-SO2-01, EP-SO2-01.temperature);
    }
#endif
}

bool XsnsXXX(uint32_t function)
{
    if (!Rs485Enabled(XRS485_31))
    {
        return false;
    }
    bool result = false;
    if (FUNC_INIT == function)
    {
        EP-SO2-01Init();
    }
    else if (EP-SO2-01.valid)
    {
        switch (function)
        {
        case FUNC_EVERY_250_MSECOND:
            EP-SO2-01ReadData();
            break;
        case FUNC_JSON_APPEND:
            EP-SO2-01Show(1);
            break;
#ifdef USE_WEBSERVER
        case FUNC_WEB_SENSOR:
            EP-SO2-01Show(0);
            break;
#endif
        }
    }
    return result;
}
#endif
#endif