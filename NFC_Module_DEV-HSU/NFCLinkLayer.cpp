#include "NFCLinkLayer.h"

#include "debug.h"

uint8_t SYMM_PDU[] = {0, 0};

NFCLinkLayer::NFCLinkLayer(NFCReader *nfcReader)
    : _nfcReader(nfcReader)
{
}

NFCLinkLayer::~NFCLinkLayer()
{

}

/*
 * the conversation of opening SNEP server link
 * 
 * <- SYMM
 * -> CONN [LLCP_CONNECTING]
 * <- SYMM [option]
 * -> SYMM [option]
 * <- CC [LLCP_CONNECTED]
 */
uint32_t NFCLinkLayer::openSNEPClientLink(void)
{
   PDU *targetPayload;
   PDU *recievedPDU;
   uint8_t PDUBuffer[64];
   uint8_t DataIn[64];
   
   targetPayload = (PDU *) PDUBuffer;
   
   DMSG(F("Opening SNEP Client Link.\n"));

   uint32_t result = _nfcReader->configurePeerAsTarget(SNEP_CLIENT);

   if (IS_ERROR(result))
   {
       return result;
   }
   
   recievedPDU = ( PDU *) DataIn;
   uint32_t rx_result = _nfcReader->targetRxData(DataIn);
   if (IS_ERROR(rx_result))
   {
     DMSG(F("Connection Failed.\n"));

     return CONNECT_RX_FAILURE;
   }

  uint8_t PDU[2] ;

  /*
   * Send CONNECTION PDU
   */
  PDU[0] = 0x11;
  PDU[1] = 0x20;
  if (IS_ERROR(_nfcReader->targetTxData(PDU, 2))) {
    return CONNECT_TX_FAILURE;
  }

  
  _nfcReader->targetRxData(DataIn);
  PDU[0] = 0;
  PDU[1] = 0;
  int count = 0;
  while (recievedPDU->getPTYPE() == SYMM_PTYPE) {
    if (IS_ERROR(_nfcReader->targetTxData(PDU, 2))) {
        return CONNECT_TX_FAILURE;
    }
    
    if (IS_ERROR(_nfcReader->targetRxData(DataIn))) {
      return CONNECT_RX_FAILURE;
    }
    
    count++;
    if (count > 10) {
      return CONNECT_TX_FAILURE;
    }
  }


  /*
   * get a CONNECTION COMPLETE PDU 
   */
   if (recievedPDU->getPTYPE() != CONNECTION_COMPLETE_PTYPE)
   {
      DMSG(F("Connection Complete Failed.\n"));

      return UNEXPECTED_PDU_FAILURE;
   }

   DSAP = recievedPDU->getSSAP();
   SSAP = recievedPDU->getDSAP();

   return RESULT_SUCCESS;
}

uint32_t NFCLinkLayer::closeSNEPClientLink()
{

}

/*
 * the conversation of opening SNEP server link
 * 
 * -> SYMM
 * <- SYMM
 * ... SYMM repeat
 * <- CONN [LLCP_CONNECTING]
 * -> CC [LLCP_CONNECTED]
 */

uint32_t NFCLinkLayer::openSNEPServerLink(void)
{
   uint8_t status[2];
   uint8_t DataIn[64];
   PDU *recievedPDU;
   PDU targetPayload;
   uint32_t result;

   DMSG(F("Opening Server Link.\n"));

   result = _nfcReader->configurePeerAsTarget(SNEP_CLIENT);
   if (IS_ERROR(result))
   {
       return result;
   }

   uint8_t symm[] = {0, 0};
   recievedPDU = (PDU *)DataIn;
   do
   {
     result = _nfcReader->targetRxData(DataIn);
     
     if (IS_ERROR(result))
     {
        return result;
     }
     
     if (recievedPDU->getPTYPE() == CONNECT_PTYPE) {
        break;
     } else {
        result = _nfcReader->targetTxData(symm, sizeof(symm));
     }
     
   } while (RESULT_OK(result));

   targetPayload.setDSAP(recievedPDU->getSSAP());
   targetPayload.setPTYPE(CONNECTION_COMPLETE_PTYPE);
   targetPayload.setSSAP(recievedPDU->getDSAP());

   if (IS_ERROR(_nfcReader->targetTxData((uint8_t *)&targetPayload, 2)))
   {
      DMSG(F("Connection Complete Failed.\n"));

      return CONNECT_COMPLETE_TX_FAILURE;
   }

   return RESULT_SUCCESS;
}

uint32_t NFCLinkLayer::closeSNEPServerLink()
{
   uint8_t DataIn[64];
   PDU *recievedPDU;

   recievedPDU = (PDU *)DataIn;

   uint32_t result = _nfcReader->targetRxData(DataIn);

   if (_nfcReader->isTargetReleasedError(result))
   {
      return RESULT_SUCCESS;
   }
   else if (IS_ERROR(result))
   {
      return result;
   }

   //Serial.println(F("Recieved disconnect Message."));

   return result;
}

uint32_t NFCLinkLayer::serverLinkRxData(uint8_t *&Data)
{
   uint8_t len;
   PDU ackPDU;
   uint32_t result;
   uint8_t count = 0;
   PDU *recievedPDU = (PDU *) Data;

   do {
    result = _nfcReader->targetRxData(Data);
    if (IS_ERROR(result))
    {
      DMSG(F("Failed to Recieve NDEF Message.\n"));

      return NDEF_MESSAGE_RX_FAILURE;
    }
    
    if (recievedPDU->getPTYPE() == INFORMATION_PTYPE)
    {
      break;
    }
    
    if (recievedPDU->getPTYPE() == SYMM_PTYPE) {
      result = _nfcReader->targetTxData(SYMM_PDU, sizeof(SYMM_PDU));
      if (IS_ERROR(result))
      {
        DMSG(F("Failed to Recieve NDEF Message.\n"));

        return NDEF_MESSAGE_RX_FAILURE;
      }
    }
    
    count++;
    if (count > 10) {
      return NDEF_MESSAGE_RX_FAILURE;
    }
   } while (1);

   len = (uint8_t) result - 2;
   
   Data = &Data[3];

   // Acknowledge reciept of Information PDU
   ackPDU.setDSAP(recievedPDU->getSSAP());
   ackPDU.setPTYPE(RECEIVE_READY_TYPE);
   ackPDU.setSSAP(recievedPDU->getDSAP());

   ackPDU.params.sequence = (recievedPDU->params.sequence  + 1)& 0x0F;

   result = _nfcReader->targetTxData((uint8_t *)&ackPDU, 3);
   if (IS_ERROR(result))
   {
      DMSG(F("Ack Failed."));

      return result;
   }
   
   
   // Receive SYMM PDU
   uint8_t rwbuf[9];
   _nfcReader->targetRxData(rwbuf, sizeof(rwbuf));
   
   // Send INFO PDU to confirm SNEP
   rwbuf[0] = (recievedPDU->getSSAP() << 2) + 0x3;
   rwbuf[1] = recievedPDU->getDSAP();
   rwbuf[2] = 0x01;
   rwbuf[3] = 0x10;
   rwbuf[4] = 0x81;
   rwbuf[5] = 0x00;
   rwbuf[6] = 0x00;
   rwbuf[7] = 0x00;
   rwbuf[8] = 0x00;
   _nfcReader->targetTxData(rwbuf, 9);

   // Receive RR PDU
   _nfcReader->targetRxData(rwbuf, sizeof(rwbuf));
   
   // Send SYMM PDU
   rwbuf[0] = 0x00;
   rwbuf[1] = 0x00;
   _nfcReader->targetTxData(rwbuf, 2);
   
   // Receive DISC PDU
   _nfcReader->targetRxData(rwbuf, sizeof(rwbuf));
   
   // Send DM PDU
   rwbuf[0] = (recievedPDU->getSSAP() << 2) + 0x1;
   rwbuf[1] = (0x1 << 6) + recievedPDU->getDSAP();
   rwbuf[2] = 0x0;
   _nfcReader->targetTxData(rwbuf, 3);

   return len;
}

uint32_t NFCLinkLayer::clientLinkTxData(uint8_t *snepMessage, uint32_t len)
{
   PDU *infoPDU = (PDU *) ALLOCATE_HEADER_SPACE(snepMessage, 3);
   infoPDU->setDSAP(DSAP);
   infoPDU->setSSAP(SSAP);
   infoPDU->setPTYPE(INFORMATION_PTYPE);

   infoPDU->params.sequence = 0;
      
   uint8_t buf[32];
      
    uint8_t symm[] = {0, 0};
    _nfcReader->targetTxData(symm, sizeof(symm));
    
//    _nfcReader->targetRxData(buf);
   

   if (IS_ERROR(_nfcReader->targetTxData((uint8_t *)infoPDU, len + 3)))
   {
     DMSG(F("Sending NDEF Message Failed."));

     return NDEF_MESSAGE_TX_FAILURE;
   }
   

   _nfcReader->targetRxData(buf);
   
   _nfcReader->targetTxData(symm, sizeof(symm));

   PDU disconnect;
   disconnect.setDSAP(DSAP);
   disconnect.setSSAP(SSAP);
   disconnect.setPTYPE(DISCONNECT_PTYPE);
   
   _nfcReader->targetTxData((uint8_t *)&disconnect, 2);
   
   _nfcReader->targetRxData(buf);

   DMSG(F("Sent NDEF Message"));

   return RESULT_SUCCESS;
}

inline bool PDU::isConnectClientRequest()
{
#if 0
    return ((getPTYPE() == CONNECT_PTYPE)                     &&
             (params.length == CONNECT_SERVICE_NAME_LEN)       &&
             (strncmp((char *)params.data, CONNECT_SERVICE_NAME, CONNECT_SERVICE_NAME_LEN) == 0));
#else
    return (getPTYPE() == CONNECT_PTYPE);
#endif
}

PDU::PDU()
{
   field[0] = 0;
   field[1] = 0;
   params.type = 0;
   params.length = 0;
}

uint8_t PDU::getDSAP()
{
   return (field[0] >> 2);
}

uint8_t PDU::getSSAP()
{
   return (field[1] & 0x3F);
}

uint8_t PDU::getPTYPE()
{
   return (((field[0] & 0x03) << 2) | ((field[1] & 0xC0) >> 6));
}

void PDU::setDSAP(uint8_t DSAP)
{
   field[0] &= 0x03; // Clear the DSAP bits
   field[0] |= ((DSAP & 0x3F) << 2);  // Set the bits
}

void PDU::setSSAP(uint8_t SSAP)
{
  field[1] &= (0xC0);    // Clear the SSAP bits
  field[1] |= (0x3F & SSAP);
}

void PDU::setPTYPE(uint8_t PTYPE)
{
   field[0] &= 0xFC; // Clear the last two bits that contain the PTYPE
   field[1] &= 0x3F; // Clear the upper two bits that contain the PTYPE
   field[0] |= (PTYPE & 0x0C) >> 2;
   field[1] |= (PTYPE & 0x03) << 6;
}
