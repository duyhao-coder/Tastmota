#ifdef USE_RS485
#ifdef USE_XX_XX

#define XSNS_XXX XXX
#define XRS485_XXX XXX

struct XXXXt
{
    bool valid = false;
    uint16_t temperature = 0.0;
    char name[5] = "XXXX";
} XXXX;

#define XXXX_ADDRESS_ID 0x00
#define XXXX_ADDRESS_VALUE 0x0111
#define XXXX_FUNCTION_CODE 0x03
#define XXXX_ADDRESS_CHECK 0x0112

bool XXXXisConnected()
{
    if (!RS485.active)
        return false;

    RS485.Rs485Modbus->Send(XXXX_ADDRESS_ID, XXXX_FUNCTION_CODE, XXXX_ADDRESS_CHECK, 0x01);

    delay(200);

    RS485.Rs485Modbus->ReceiveReady();

    uint8_t buffer[8];
    uint8_t error = RS485.Rs485Modbus->ReceiveBuffer(buffer, 8);

    if (error)
    {
        AddLog(LOG_LEVEL_INFO, PSTR("XXXX has error %d"), error);
        return false;
    }
    else
    {
        uint16_t check_XXXX = (buffer[3] << 8) | buffer[4];
        if (check_XXXX == XXXX_ADDRESS_ID)
            return true;
    }
    return false;
}

void XXXXInit()
{
    if (!RS485.active)
        return;
    XXXX.valid = XXXXisConnected();
    if (XXXX.valid)
        Rs485SetActiveFound(XXXX_ADDRESS_ID, XXXX.name);
    AddLog(LOG_LEVEL_INFO, PSTR(XXXX.valid ? "XXXX is connected" : "XXXX is not detected"));
}

const char HTTP_SNS_XXXX[] PROGMEM = "{s} Temperature {m} %.1f";
#define D_JSON_XXXX "XXXX"

void XXXXReadData()
{
    if (!XXXX.valid)
        return;

    if (isWaitingResponse(XXXX_ADDRESS_ID))
        return;

    if ((RS485.requestSent[XXXX_ADDRESS_ID] == 0) && (RS485.lastRequestTime == 0))
    {
        RS485.Rs485Modbus->Send(XXXX_ADDRESS_ID, XXXX_FUNCTION_CODE, XXXX_ADDRESS_VALUE, 1);
        RS485.requestSent[XXXX_ADDRESS_ID] = 1;
        RS485.lastRequestTime = millis();
    }

    if ((RS485.requestSent[XXXX_ADDRESS_ID] == 1) && (millis() - RS485.lastRequestTime >= 200))
    {
        if (!RS485.Rs485Modbus->ReceiveReady())
            return;
        uint8_t buffer[8];
        uint8_t error = RS485.Rs485Modbus->ReceiveBuffer(buffer, 8);

        if (error)
        {
            AddLog(LOG_LEVEL_INFO, PSTR("Modbus XXXX error: %d"), error);
        }
        else
        {
            uint16_t temperatureRaw = (buffer[3] << 8) | buffer[4];
            XXXX.temperature = temperatureRaw;
        }
        RS485.requestSent[XXXX_ADDRESS_ID] = 0;
        RS485.lastRequestTime = 0;
    }
}

void XXXXShow(bool json)
{
    if (json)
    {
        ResponseAppend_P(PSTR(",\"%s\":{"), XXXX.name);
        ResponseAppend_P(PSTR("\"" D_JSON_XXXX "\":%.1f"), XXXX.temperature);
        ResponseJsonEnd();
    }
#ifdef USE_WEBSERVER
    else
    {
        WSContentSend_PD(HTTP_SNS_XXXX, XXXX.temperature);
    }
#endif
}

bool XsnsXXX(uint32_t function)
{
    if (!Rs485Enabled(XRS485_XX))
    {
        return false;
    }
    bool result = false;
    if (FUNC_INIT == function)
    {
        XXXXInit();
    }
    else if (XXXX.valid)
    {
        switch (function)
        {
        case FUNC_EVERY_250_MSECOND:
            XXXXReadData();
            break;
        case FUNC_JSON_APPEND:
            XXXXShow(1);
            break;
#ifdef USE_WEBSERVER
        case FUNC_WEB_SENSOR:
            XXXXShow(0);
            break;
#endif
        }
    }
    return result;
}
#endif
#endif