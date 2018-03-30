// PN532 library by adafruit/ladyada
// MIT license

// authenticateBlock, readMemoryBlock, writeMemoryBlock contributed
// by Seeed Technology Inc (www.seeedstudio.com)

#include "PN5321.h"

#include "debug.h"

uint8_t pn532ack[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
uint8_t pn532response_firmwarevers[] = {0x00, 0xFF, 0x06, 0xFA, 0xD5, 0x03};

#define COMMAND_RESPONSE_SIZE 3
#define TS_GET_DATA_IN_MAX_SIZE  262 + 3

uint8_t pn532_packetbuffer[TS_GET_DATA_IN_MAX_SIZE];

PN532::PN532(HardwareSerial &serial)
{
    _serial = &serial;
    command = 0;
}

void PN532::initializeReader()
{
    _serial -> begin(115200);
    wakeup();

    getFirmwareVersion();
}

void PN532::wakeup()
{
    _serial->write(0x55);
    _serial->write(0x55);
    _serial->write(0);
    _serial->write(0);
    _serial->write(0);

    /** dump serial buffer */
    if(_serial->available()){
        DMSG("Dump serial buffer: ");
    }
    while(_serial->available()){
        uint8_t ret = _serial->read();
        DMSG_HEX(ret);
    }
}

int8_t PN532::receive(uint8_t *buf, int len, uint16_t timeout)
{
  int read_bytes = 0;
  int ret;
  unsigned long start_millis;
  
  while (read_bytes < len) {
    start_millis = millis();
    do {
      ret = _serial->read();
      if (ret >= 0) {
        break;
     }
    } while( (millis()- start_millis ) < timeout || !timeout);
    
    if (ret < 0) {
        if(read_bytes){
            return read_bytes;
        }else{
            return -1;
        }
    }
    buf[read_bytes] = (uint8_t)ret;
    DMSG_HEX(ret);
    read_bytes++;
  }
  return read_bytes;
}

int8_t PN532::readAckFrame(uint16_t timeout)
{
    const uint8_t PN532_ACK[] = {0, 0, 0xFF, 0, 0xFF, 0};
    uint8_t ackBuf[sizeof(PN532_ACK)];
    
    DMSG("Read Ack\n");
    
    if( receive(ackBuf, sizeof(PN532_ACK), timeout) <= 0 ){
        DMSG("Read ACK Timeout\n");
        return SEND_COMMAND_RX_TIMEOUT_ERROR;
    }
    
    if( memcmp(ackBuf, PN532_ACK, sizeof(PN532_ACK)) ){
        DMSG("Invalid ACK\n");
        return SEND_COMMAND_RX_ACK_ERROR;
    }
    return RESULT_SUCCESS;
}

// default timeout of one second
uint32_t PN532::sendCommandCheckAck(uint8_t *cmd,
                                    uint8_t cmdlen,
                                    uint16_t timeout)
{
 
    /** dump serial buffer */
    if(_serial->available()){
        DMSG("Dump serial buffer: ");
    }
    while(_serial->available()){
        uint8_t ret = _serial->read();
        DMSG_HEX(ret);
    }

    command = cmd[0];
    
    _serial->write(PN532_PREAMBLE);
    _serial->write(PN532_STARTCODE1);
    _serial->write(PN532_STARTCODE2);
    
    uint8_t length = cmdlen + 1;   // length of data field: TFI + DATA
    _serial->write(length);
    _serial->write(~length + 1);         // checksum of length
    
    _serial->write(PN532_HOSTTOPN532);
    uint8_t sum = PN532_HOSTTOPN532;    // sum of TFI + DATA
    
    _serial->write(cmd, cmdlen);
    for (uint8_t i = 0; i < cmdlen; i++) {
        sum += cmd[i];
    }
    
    uint8_t checksum = ~sum + 1;            // checksum of TFI + DATA
    _serial->write(checksum);
    _serial->write(PN532_POSTAMBLE);

    return readAckFrame(timeout);
}

uint32_t PN532::readspicommand(uint8_t cmdCode, PN532_CMD_RESPONSE *response,  uint16_t timeout)
{
    uint8_t tmp[3];
    
    DMSG("Read response: ");
    
    /** Frame Preamble and Start Code */
    if(receive(tmp, 3, timeout)<=0){
        return SEND_COMMAND_RX_TIMEOUT_ERROR;
    }
    if(0 != tmp[0] || 0!= tmp[1] || 0xFF != tmp[2]){
        DMSG("Preamble error");
        return INVALID_RESPONSE;
    }
    response->header[0] = tmp[1];
    response->header[1] = tmp[2];


    /** receive length and check */
    uint8_t length[2];
    if(receive(length, 2, timeout) <= 0){
        return SEND_COMMAND_RX_TIMEOUT_ERROR;
    }
    if( 0 != (uint8_t)(length[0] + length[1]) ){
        DMSG("Length error");
        return INVALID_RESPONSE;
    }
    response->len = length[0];
    response->len_chksum = length[1];
    response->data_len = length[0] - 2;
    
    /** receive command byte */
    uint8_t cmd = cmdCode+1;               // response command
    if(receive(tmp, 2, timeout) <= 0){
        return SEND_COMMAND_RX_TIMEOUT_ERROR;
    }
    if( PN532_PN532TOHOST != tmp[0] || cmd != tmp[1]){
        DMSG("Command error");
        return INVALID_RESPONSE;
    }
    response->direction = tmp[0];
    response->responseCode = tmp[1];

    if(receive(response->data, response->data_len, timeout) != response->data_len){
        return SEND_COMMAND_RX_TIMEOUT_ERROR;
    }
    uint8_t sum = PN532_PN532TOHOST + cmd;
    for(uint8_t i=0; i<response->data_len; i++){
        sum += response->data[i];
    }
    
    /** checksum and postamble */
    if(receive(tmp, 2, timeout) <= 0){
        return SEND_COMMAND_RX_TIMEOUT_ERROR;
    }
    if( 0 != (uint8_t)(sum + tmp[0]) || 0 != tmp[1] ){
        DMSG("Checksum error");
        return INVALID_POSTAMBLE;
    }
    
    DMSG("\n");

    return RESULT_SUCCESS;
}

/****************************************************************************/
uint32_t PN532::getFirmwareVersion(void)
{
    uint32_t version;

    DMSG("Get Version\n");

    pn532_packetbuffer[0] = PN532_FIRMWAREVERSION;

    if (IS_ERROR(sendCommandCheckAck(pn532_packetbuffer, 1)))
    {
        DMSG("Ack Failed\n");
        return 0;
    }

    // read response Packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    if (IS_ERROR(readspicommand(PN532_FIRMWAREVERSION, response)))
    {
        DMSG("Response Failed\n");
        return 0;
    }

    //response->printResponse();

    version = response->data[0];
    version <<= 8;
    version |= response->data[1];
    version <<= 8;
    version |= response->data[2];
    version <<= 8;
    version |= response->data[3];

    return version;
}

uint32_t PN532::SAMConfig(void)
{
    pn532_packetbuffer[0] = PN532_SAMCONFIGURATION;
    pn532_packetbuffer[1] = 0x01; // normal mode;
    pn532_packetbuffer[2] = 0x14; // timeout 50ms * 20 = 1 second
    pn532_packetbuffer[3] = 0x01; // use IRQ pin!

    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, 4);

    if (IS_ERROR(result))
    {
        return result;
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    return readspicommand(PN532_SAMCONFIGURATION, response);
}

uint32_t PN532::configurePeerAsInitiator(uint8_t baudrate)
{
    // Does not support 106
    if (baudrate != NFC_READER_CFG_BAUDRATE_201_KPS && baudrate != NFC_READER_CFG_BAUDRATE_424_KPS)
    {
       return CONFIGURE_HARDWARE_ERROR;
    }

    pn532_packetbuffer[0] = PN532_INJUMPFORDEP;
    pn532_packetbuffer[1] = 0x01; //Active Mode
    pn532_packetbuffer[2] = baudrate; // Use 1 or 2. //0 i.e 106kps is not supported yet
    pn532_packetbuffer[3] = 0x01; //Indicates Optional Payload is present

    //Polling request payload
    pn532_packetbuffer[4] = 0x00;
    pn532_packetbuffer[5] = 0xFF;
    pn532_packetbuffer[6] = 0xFF;
    pn532_packetbuffer[7] = 0x00;
    pn532_packetbuffer[8] = 0x00;

    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, 9);

    if (IS_ERROR(result))
    {
        return result;
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    result = readspicommand(PN532_INJUMPFORDEP,  response);
    if (IS_ERROR(result))
    {
       return result;
    }

    if (response->data[0] != 0x00)
    {
       return (GEN_ERROR | response->data[0]);
    }

    return RESULT_SUCCESS; //No error
}


uint32_t PN532::initiatorTxRxData(uint8_t *DataOut,
                           uint32_t dataSize,
                           uint8_t *DataIn)
{
    pn532_packetbuffer[0] = PN532_INDATAEXCHANGE;
    pn532_packetbuffer[1] = 0x01; //Target 01

    for(uint8_t iter=(2+0);iter<(2+dataSize);iter++)
    {
        pn532_packetbuffer[iter] = DataOut[iter-2]; //pack the data to send to target
    }

    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, dataSize+2);

    if (IS_ERROR(result))
    {
        return result;
    }

    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    if (!readspicommand(PN532_INDATAEXCHANGE, response))
    {
       return false;
    }

#if IS_DEBUG
    response->printResponse();
#endif

    if (response->data[0] != 0x00)
    {
       return (GEN_ERROR | response->data[0]);
    }
    return RESULT_SUCCESS; //No error
}

uint32_t PN532::configurePeerAsTarget(uint8_t type)
{
    static const uint8_t command[] =      { PN532_TGINITASTARGET,
            0,
            0x00, 0x00,         //SENS_RES
            0x00, 0x00, 0x00,   //NFCID1
            0x40,               //SEL_RES

            0x01, 0xFE, 0x0F, 0xBB, 0xBA, 0xA6, 0xC9, 0x89, // POL_RES
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF,

            0x01, 0xFE, 0x0F, 0xBB, 0xBA, 0xA6, 0xC9, 0x89, 0x00, 0x00, //NFCID3t: Change this to desired value

            0x06, 0x46,  0x66, 0x6D, 0x01, 0x01, 0x10, 0x00// LLCP magic number and version parameter
            };


    uint32_t result;
    result = sendCommandCheckAck((uint8_t *)command, sizeof(command));

    if (IS_ERROR(result))
    {
        return result;
    }

    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    return readspicommand(PN532_TGINITASTARGET, response);
}

uint32_t PN532::getTargetStatus(uint8_t *DataIn)
{
    pn532_packetbuffer[0] = PN532_TGTARGETSTATUS;

    if (IS_ERROR(sendCommandCheckAck(pn532_packetbuffer, 1))) {
        return 0;
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    if (RESULT_OK(readspicommand(PN532_TGTARGETSTATUS, response)))
    {
       memcpy(DataIn, response->data, response->data_len);
       return response->data_len;
    }

    return 0;
}

uint32_t PN532::targetRxData(uint8_t *DataIn)
{
    ///////////////////////////////////// Receiving from Initiator ///////////////////////////
    pn532_packetbuffer[0] = PN532_TGGETDATA;
    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, 1, 1000);
    if (IS_ERROR(result)) {
        //Serial.println(F("SendCommandCheck Ack Failed"));
        return NFC_READER_COMMAND_FAILURE;
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;

    result = readspicommand(PN532_TGGETDATA, response);

    if (IS_ERROR(result))
    {
       return NFC_READER_RESPONSE_FAILURE;
    }

    if (response->data[0] == 0x00)
    {
       uint32_t ret_len = response->data_len - 1;
       memcpy(DataIn, &(response->data[1]), ret_len);
       return ret_len;
    }

    return (GEN_ERROR | response->data[0]);
}

uint32_t PN532::targetRxData(uint8_t *DataIn, uint32_t DataSize)
{
    ///////////////////////////////////// Receiving from Initiator ///////////////////////////
    pn532_packetbuffer[0] = PN532_TGGETDATA;
    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, 1, 1000);
    if (IS_ERROR(result)) {
        //Serial.println(F("SendCommandCheck Ack Failed"));
        return NFC_READER_COMMAND_FAILURE;
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;

    result = readspicommand(PN532_TGGETDATA, response);

    if (IS_ERROR(result))
    {
       return NFC_READER_RESPONSE_FAILURE;
    }

    if (response->data[0] == 0x00)
    {
       uint32_t ret_len = response->data_len - 1;
       if (ret_len > DataSize) {
           ret_len = DataSize;
       }
       memcpy(DataIn, &(response->data[1]), ret_len);
       return ret_len;
    }

    return (GEN_ERROR | response->data[0]);
}



uint32_t PN532::targetTxData(uint8_t *DataOut, uint32_t dataSize)
{
    ///////////////////////////////////// Sending to Initiator ///////////////////////////
    pn532_packetbuffer[0] = PN532_TGSETDATA;
    uint8_t commandBufferSize = (1 + dataSize);
    for(uint8_t iter=(1+0);iter < commandBufferSize; ++iter)
    {
        pn532_packetbuffer[iter] = DataOut[iter-1]; //pack the data to send to target
    }

    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, commandBufferSize, 1000);
    if (IS_ERROR(result)) {
        DMSG(F("TX_Target Command Failed."));

        return result;
    }


    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;

    result = readspicommand(PN532_TGSETDATA, response);
    if (IS_ERROR(result))
    {
       return result;
    }

    if (response->data[0] != 0x00)
    {
       return (GEN_ERROR | response->data[0]);
    }
    return RESULT_SUCCESS; //No error
}


uint32_t PN532::authenticateBlock(uint8_t cardnumber /*1 or 2*/,
                                  uint32_t cid /*Card NUID*/,
                                  uint8_t blockaddress /*0 to 63*/,
                                  uint8_t authtype/*Either KEY_A or KEY_B */,
                                  uint8_t * keys)
{
    pn532_packetbuffer[0] = PN532_INDATAEXCHANGE;
    pn532_packetbuffer[1] = cardnumber;  // either card 1 or 2 (tested for card 1)
    if(authtype == KEY_A)
    {
        pn532_packetbuffer[2] = PN532_AUTH_WITH_KEYA;
    }
    else
    {
        pn532_packetbuffer[2] = PN532_AUTH_WITH_KEYB;
    }
    pn532_packetbuffer[3] = blockaddress; //This address can be 0-63 for MIFARE 1K card

    pn532_packetbuffer[4] = keys[0];
    pn532_packetbuffer[5] = keys[1];
    pn532_packetbuffer[6] = keys[2];
    pn532_packetbuffer[7] = keys[3];
    pn532_packetbuffer[8] = keys[4];
    pn532_packetbuffer[9] = keys[5];

    pn532_packetbuffer[10] = ((cid >> 24) & 0xFF);
    pn532_packetbuffer[11] = ((cid >> 16) & 0xFF);
    pn532_packetbuffer[12] = ((cid >> 8) & 0xFF);
    pn532_packetbuffer[13] = ((cid >> 0) & 0xFF);

    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, 14);

    if (IS_ERROR(result))
    {
        return result;
    }

    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    result = readspicommand(PN532_INDATAEXCHANGE, response);
    if (IS_ERROR(result))
    {
       return result;
    }


    if((response->data[0] == 0x41) && (response->data[1] == 0x00))
    {
  	    return RESULT_SUCCESS;
    }

    return (GEN_ERROR | response->data[1]);
}

uint32_t PN532::readMemoryBlock(uint8_t cardnumber /*1 or 2*/,
                                uint8_t blockaddress /*0 to 63*/,
                                uint8_t * block)
{
    pn532_packetbuffer[0] = PN532_INDATAEXCHANGE;
    pn532_packetbuffer[1] = cardnumber;  // either card 1 or 2 (tested for card 1)
    pn532_packetbuffer[2] = PN532_MIFARE_READ;
    pn532_packetbuffer[3] = blockaddress; //This address can be 0-63 for MIFARE 1K card

    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, 4);

    if (IS_ERROR(result)) {
        return result;
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    result = readspicommand(PN532_INDATAEXCHANGE, response);
    if (IS_ERROR(result))
    {
       return result;
    }

    if((response->data[0] == 0x41) && (response->data[1] == 0x00))
    {
  	    return RESULT_SUCCESS; //read successful
    }

    return (GEN_ERROR | response->data[1]);
}

//Do not write to Sector Trailer Block unless you know what you are doing.
uint32_t PN532::writeMemoryBlock(uint8_t cardnumber /*1 or 2*/,
                                 uint8_t blockaddress /*0 to 63*/,
                                 uint8_t * block)
{
    pn532_packetbuffer[0] = PN532_INDATAEXCHANGE;
    pn532_packetbuffer[1] = cardnumber;  // either card 1 or 2 (tested for card 1)
    pn532_packetbuffer[2] = PN532_MIFARE_WRITE;
    pn532_packetbuffer[3] = blockaddress;

    for(uint8_t i =0; i <16; i++)
    {
        pn532_packetbuffer[4+i] = block[i];
    }

    uint32_t result = sendCommandCheckAck(pn532_packetbuffer, 20);

    if (IS_ERROR(result))
    {
        return result;
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    result = readspicommand(PN532_INDATAEXCHANGE, response);
    if (IS_ERROR(result))
    {
       return result;
    }

    if((response->data[0] == 0x41) && (response->data[1] == 0x00))
    {
  	    return RESULT_SUCCESS; //read successful
    }

    return (GEN_ERROR | response->data[1]);
}

uint32_t PN532::readPassiveTargetID(uint8_t cardbaudrate)
{
    uint32_t cid;

    pn532_packetbuffer[0] = PN532_INLISTPASSIVETARGET;
    pn532_packetbuffer[1] = 1;  // max 1 cards at once (we can set this to 2 later)
    pn532_packetbuffer[2] = cardbaudrate;

    if (IS_ERROR(sendCommandCheckAck(pn532_packetbuffer, 3)))
    {
        return 0;  // no cards read
    }

    // read data packet
    PN532_CMD_RESPONSE *response = (PN532_CMD_RESPONSE *) pn532_packetbuffer;
    if (IS_ERROR(readspicommand(PN532_INDATAEXCHANGE, response)))
    {
       return 0;
    }

    // check some basic stuff
    Serial.print(F("Found "));
    Serial.print(response->data[2], DEC);
    Serial.println(F(" tags"));

    if (response->data[2] != 1)
    {
        return 0;
    }

    uint16_t sens_res = response->data[4];
    sens_res <<= 8;
    sens_res |= response->data[5];

    Serial.print(F("Sens Response: 0x"));
    Serial.println(sens_res, HEX);
    Serial.print(F("Sel Response: 0x"));
    Serial.println(response->data[6], HEX);

    cid = 0;
    for (uint8_t i = 0; i < response->data[7]; i++)
    {
        cid <<= 8;
        cid |= response->data[8 + i];
        Serial.print(F(" 0x"));
        Serial.print(response->data[8 + i], HEX);
    }

    return cid;
}


inline boolean PN532::isTargetReleasedError(uint32_t result)
{
   return result == (GEN_ERROR | TARGET_RELEASED_ERROR);
}

boolean PN532_CMD_RESPONSE::verifyResponse(uint32_t cmdCode)
{
    return ( header[0] == 0x00 &&
             header[1] == 0xFF &&
            ((uint8_t)(len + len_chksum)) == 0x00 &&
            direction == 0xD5 &&
            (cmdCode + 1) == responseCode);
}

void PN532_CMD_RESPONSE::printResponse()
{
    Serial.print("response: ");

    uint8_t *ptr = (uint8_t *) this;
    for (uint8_t i = 0; i < sizeof(PN532_CMD_RESPONSE); i++)
    {
        if ((ptr + i) == &data_len)
        {
            continue;
        }
        Serial.print(ptr[i], HEX);
        Serial.print(' ');
    }

    for (uint8_t i = 0; i < data_len; ++i)
    {
        Serial.print(data[i], HEX);
        Serial.print(' ');
    }
    Serial.println();
}
