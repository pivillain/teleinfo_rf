
  /*=======================================================================================================================
                                                        my_teleinfo
                                                 (c) 2012-2013 by
                                                  Script name : my_teleinfo
                                 http://www.domotique-info.fr/2014/05/recuperer-teleinformation-arduino/

                VERSIONS HISTORY
  Version 1.00  30/11/2013  + Original version
  Version 1.10    03/05/2015      + Manu : Small ajustment to variabilise the PIN numbers for Transmiter and Teleinfo
          See ref here : http://domotique.web2diz.net/?p=11#
  montage électronique conforme à http://www.domotique-info.fr/2014/05/recuperer-teleinformation-arduino/
  ======================================================================================================================*/
  /*======================================================================================================================
   * Adaptations pour photovoltaique Trame Teleinfo pas de données dans HPHC mais dans base
   * Analyse de la trame teleinfo basee sur String
  ======================================================================================================================*/



#include <SoftwareSerial.h>
// PIN SIETTINGS //
const byte TELEINFO_PIN_Prod = 6;   //Connexion TELEINFO prod
const byte TELEINFO_PIN_Conso = 5;   //Connexion TELEINFO conso
const byte TX_PIN = 4;         //emetteur 433 MHZ

const byte VCC_PIN_TX = 11; //alimentation emetteur
const byte VCC_PIN_PROD = 10; //alimentation emetteur
const byte VCC_PIN_CONSO = 9; //alimentation emetteur

// PIN SIETTINGS //

const unsigned long TIME = 488;
const unsigned long TWOTIME = TIME * 2;

#define SEND_HIGH() digitalWrite(TX_PIN, HIGH)
#define SEND_LOW() digitalWrite(TX_PIN, LOW)
byte OregonMessageBuffer[13];  //  OWL180
//*********************************************************
SoftwareSerial* mySerial;
SoftwareSerial* Conso;
SoftwareSerial* Prod;
char HHPHC;
int ISOUSC;             // intensité souscrite
int IINST;              // intensité instantanée en A
int IMAX;               // intensité maxi en A
int PAPP;               // puissance apparente en VA
unsigned long HCHC;     // compteur Heures Creuses en W
unsigned long HCHP;     // compteur Heures Pleines en W
unsigned long BASE;     // Index compteur en Wh
String PTEC;            // Régime actuel : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
String ADCO;            // adresse compteur
String OPTARIF;         // option tarifaire
String MOTDETAT;        // status word
String pgmVersion;      // TeleInfo program version
boolean ethernetIsOK;
boolean teleInfoReceived;


char chksum(char *buff, uint8_t len);
boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber);
char version[17] = "TeleInfo  V 1.20";

unsigned long PAPP_arrondi;               // PAPP*497/500/16 arrondi
unsigned long chksum_CM180;
unsigned long long HCP;

/**
 * \brief    Send logical "0" over RF
 * \details  azero bit be represented by an off-to-on transition
 * \         of the RF signal at the middle of a clock period.
 * \         Remenber, the Oregon v2.1 protocol add an inverted bit first 
 */
inline void sendZero(void) 
{
  SEND_LOW();
  delayMicroseconds(TIME); 
  SEND_HIGH();
  delayMicroseconds(TIME);
}

/**
 * \brief    Send logical "1" over RF
 * \details  a one bit be represented by an on-to-off transition
 * \         of the RF signal at the middle of a clock period.
 * \         Remenber, the Oregon v2.1 protocol add an inverted bit first 
 */
inline void sendOne(void) 
{
   SEND_HIGH();
   delayMicroseconds(TIME);
   SEND_LOW();
   delayMicroseconds(TIME);
}


/**
 * \brief    Send a buffer over RF
 * \param    data   Data to send
 * \param    size   size of data to send
 */
void sendData(byte *data, byte size)
{
  for(byte i = 0; i < size; ++i)
  {
  (bitRead(data[i], 0)) ? sendOne() : sendZero();
  (bitRead(data[i], 1)) ? sendOne() : sendZero();
  (bitRead(data[i], 2)) ? sendOne() : sendZero();
  (bitRead(data[i], 3)) ? sendOne() : sendZero();
  (bitRead(data[i], 4)) ? sendOne() : sendZero();
  (bitRead(data[i], 5)) ? sendOne() : sendZero();
  (bitRead(data[i], 6)) ? sendOne() : sendZero();
  (bitRead(data[i], 7)) ? sendOne() : sendZero();    
  }
}

/**
 * \brief    Send an Oregon message
 * \param    data   The Oregon message
 */
void sendOregon(byte *data, byte size)
{
    sendPreamble();
    sendData(data, size);   //mettre 13 a la place de   "size"
    sendPostamble();
}

/**
 * \brief    Send preamble
 * \details  The preamble : 10 pulse Ã  1 suffit pour RFXCOM 
 */
inline void sendPreamble(void)
{  
  for(byte i = 0; i < 10; ++i)   //OREGON V3 
  {
    sendOne();
  } 
}

/**
 * \brief    Send postamble
 * \details  The postamble consists of 4 "0" bits
 */
inline void sendPostamble(void)
{
 for(byte i = 0; i <4 ; ++i)   
  {
    sendZero() ;
  }

  // Ajout suite commentaire http://connectingstuff.net/blog/encodage-protocoles-oregon-scientific-sur-arduino/#comment-61955 
  SEND_LOW();
  delayMicroseconds(TIME);
}


/******************************************************************/

//=================================================================================================================
// Basic constructor
//=================================================================================================================
void TeleInfo(String version)
{
  //Serial.begin(1200,SERIAL_7E1);
  Conso = new SoftwareSerial(TELEINFO_PIN_Conso, 9); // RX, TX
  Conso->begin(1200);
  Prod = new SoftwareSerial(TELEINFO_PIN_Prod, 9); // RX, TX
  Prod->begin(1200);

  pgmVersion = version;

  // variables initializations
  /*
    1 ADCO = "270622224349";
    2 OPTARIF = "----";
    3 ISOUSC = 0;
    4 HCHC = 0L;  // compteur Heures Creuses en W
    5 HCHP = 0L;  // compteur Heures Pleines en W
    6 PTEC = "----";    // Régime actuel : HPJB, HCJB, HPJW, HCJW, HPJR, HCJR
    7 HHPHC = '-';
    8 IINST = 0;        // intensité instantanée en A
    9 IMAX = 0;         // intensité maxi en A
    10 PAPP = 0;         // puissance apparente en VA
    11 MOTDETAT = "------";
  */
}

//=================================================================================================================
// Capture des trames de Teleinfo
//=================================================================================================================
boolean readTeleInfo(boolean ethernetIsConnected)
{
#define startFrame 0x02
#define endFrame 0x03
#define startLine 0x0A
#define endLine 0x0D
#define maxFrameLen 280


  int comptChar = 0; // variable de comptage des caractères reçus
  char charIn = 0; // variable de mémorisation du caractère courant en réception

  char bufferTeleinfo[21] = "";
  int bufferLen = 0;
  int checkSum;

  ethernetIsOK = ethernetIsConnected;

  if (ethernetIsOK == true)
  {
    mySerial=Prod;
  }
  else
  {
    mySerial=Conso;
  }

  int sequenceNumnber = 0;   // number of information group

  //--- wait for starting frame character
  while (charIn != startFrame)
  { // "Start Text" STX (002 h) is the beginning of the frame
    if (mySerial->available())
      charIn = mySerial->read() & 0x7F; // Serial.read() vide buffer au fur et à mesure
      //Serial.print(charIn);
  } // fin while (tant que) pas caractère 0x02
  //  while (charIn != endFrame and comptChar<=maxFrameLen)
  while (charIn != endFrame)
  { // tant que des octets sont disponibles en lecture : on lit les caractères
    // if (Serial.available())
    if (mySerial->available())
    {
      charIn = mySerial->read() & 0x7F;
      // incrémente le compteur de caractère reçus
      comptChar++;
      if (charIn == startLine)
        bufferLen = 0;
      bufferTeleinfo[bufferLen] = charIn;
      // on utilise une limite max pour éviter String trop long en cas erreur réception
      // ajoute le caractère reçu au String pour les N premiers caractères
      if (charIn == endLine)
      {
        checkSum = bufferTeleinfo[bufferLen - 1];
        if (chksum(bufferTeleinfo, bufferLen) == checkSum)
        { // we clear the 1st character (on a écrit StartLine dans le bufferTeleinfo)
          strncpy(&bufferTeleinfo[0], &bufferTeleinfo[1], bufferLen - 3);
          //caractere fin de chaine C
          bufferTeleinfo[bufferLen - 3] =  0x00;
          sequenceNumnber++;
          //traitement du paquet
          if (! handleBuffer(bufferTeleinfo, sequenceNumnber))
          {
            Serial.println(F("Sequence error ..."));
            return false;
          }
        }
        else
        {
          Serial.println(F("Checksum error ..."));
          return false;
        }
      }
      else
        bufferLen++;
    }
    if (comptChar > maxFrameLen)
    {
      Serial.println(F("Overflow error ..."));
      return false;
    }
  }
  return true;
}

//=================================================================================================================
// Frame parsing
//=================================================================================================================
//void handleBuffer(char *bufferTeleinfo, uint8_t len)
boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber)
{
  // create a pointer to the first char after the space
  char* resultString = strchr(bufferTeleinfo, ' ') + 1;
  String Label, Value, Buffer;
  boolean sequenceIsOK = true;

  Buffer = bufferTeleinfo;
  int BufLg=Buffer.length();
  int Idx=Buffer.indexOf(" ");
  int Lg=BufLg-Idx;
  
  Label = Buffer.substring(0,Idx);
  Value = Buffer.substring(Idx + 1);
  /*
Serial.print(F("LG = "));
Serial.print(BufLg);
Serial.print(F(" Idx = "));
Serial.print(Idx);
Serial.print(F(" Label = *"));
Serial.print(Label);
Serial.print(F("* Value = *"));
Serial.print(Value);
Serial.println(F("*"));
  */
  if (Label.equalsIgnoreCase("ADCO")) {
    ADCO = Value;
  }
  if (Label.equalsIgnoreCase("OPTARIF")) {
    OPTARIF = Value;
  }
  if (Label.equalsIgnoreCase("ISOUSC")) {
    ISOUSC = Value.toInt();
  }
 if (Label.equalsIgnoreCase("BASE")) {
    BASE = Value.toFloat();
  }
  if (Label.equalsIgnoreCase("HCHC")) {
    HCHC = Value.toFloat();
  }
  if (Label.equalsIgnoreCase("HCHP")) {
    HCHP = Value.toFloat();
  }
  /*
  if (Label.equalsIgnoreCase("EJPHN")) {}
  if (Label.equalsIgnoreCase("EJPHPM")) {}
  if (Label.equalsIgnoreCase("BBRHCJB")) {}
  if (Label.equalsIgnoreCase("BBRHPJB")) {}
  if (Label.equalsIgnoreCase("BBRHCJW")) {}
  if (Label.equalsIgnoreCase("BBRHPJW")) {}
  if (Label.equalsIgnoreCase("BBRHCJR")) {}
  if (Label.equalsIgnoreCase("BBRHPJR")) {}
  if (Label.equalsIgnoreCase("PEJP")) {}
  */
  if (Label.equalsIgnoreCase("PTEC")) {
    PTEC = Value;
  }
 // if (Label.equalsIgnoreCase("DEMAIN")) {}
  if (Label.equalsIgnoreCase("IINST")) {
    IINST = Value.toInt();
  }
 // if (Label.equalsIgnoreCase("ADPS")) {}
  if (Label.equalsIgnoreCase("IMAX")) {
    IMAX = Value.toInt();
  }
  if (Label.equalsIgnoreCase("PAPP")) {
    PAPP = Value.toInt();
  }
  if (Label.equalsIgnoreCase("HHPHC")) {
    HHPHC = Value[0];
  }
 if (Label.equalsIgnoreCase("MOTDETAT")) {
    MOTDETAT=Value;
  }
 
  Serial.print(F("LABEL : "));
  Serial.print(Label);
  Serial.print(F(" : Valeur : "));
  Serial.println(Value);
 
  /*
    switch(sequenceNumnber)
    {
    case 1:
      if (bufferTeleinfo[0]=='A')
      {
        ADCO = String(resultString);
        Serial.print(F("ADCO_PV : "));
        Serial.print(resultString);
        Serial.print(F(" : ADCO : "));
        Serial.println(ADCO);
        sequenceIsOK=true;
      }
      break;
    case 2:
      if (bufferTeleinfo[0]=='O')
      {
        OPTARIF = String(resultString);
        sequenceIsOK=true;
      }
      break;
    case 3:
      if (bufferTeleinfo[1]=='S')
      {
        ISOUSC = atol(resultString);
        sequenceIsOK=true;
      }
      break;
    case 4:
      if (bufferTeleinfo[3]=='C')
      {
        HCHC = atol(resultString);
        sequenceIsOK=true;
      }
      break;
    case 5:
      if (bufferTeleinfo[3]=='P')
      {
        HCHP = atol(resultString);
        sequenceIsOK=true;
      }
      break;
    case 6:
      if (bufferTeleinfo[1]=='T')
      {
        PTEC = String(resultString);
        sequenceIsOK=true;
      }
      break;
    case 7:
      if (bufferTeleinfo[1]=='I')
      {
        IINST =atol(resultString);
        sequenceIsOK=true;
      }
      break;
    case 8:
      if (bufferTeleinfo[1]=='M')
      {
        IMAX =atol(resultString);
        sequenceIsOK=true;
      }
      break;
    case 9:
      if (bufferTeleinfo[1]=='A')
      {
        PAPP =atol(resultString);
        sequenceIsOK=true;
      }
      break;
    case 10:
      if (bufferTeleinfo[1]=='H')
      {
        HHPHC = resultString[0];
        sequenceIsOK=true;
      }
      break;
    case 11:
      if (bufferTeleinfo[1]=='O')
      {
        MOTDETAT = String(resultString);
        sequenceIsOK=true;
      }
      break;
    }
    #ifdef debug
    if(!sequenceIsOK)
    {
      Serial.print(F("Out of sequence ... "));
      Serial.println(bufferTeleinfo);
    }
    #endif
  */
  return sequenceIsOK;
}

//=================================================================================================================
// Calculates teleinfo Checksum
//=================================================================================================================
char chksum(char *buff, uint8_t len)
{
  int i;
  char sum = 0;
  for (i = 1; i < (len - 2); i++)
    sum = sum + buff[i];
  sum = (sum & 0x3F) + 0x20;
  return (sum);
}


//=================================================================================================================
// This function displays the TeleInfo Internal counters
// It's usefull for debug purpose
//=================================================================================================================
void displayTeleInfo()
{
  /*
    ADCO 270622224349 B
    OPTARIF HC.. <
    ISOUSC 30 9
    HCHC 014460852 $
    HCHP 012506372 -
    PTEC HP..
    IINST 002 Y
    IMAX 035 G
    PAPP 00520 (
    HHPHC C .
    MOTDETAT 000000 B
  */

  Serial.print(F(" "));
  Serial.println();
  Serial.print(F("ADCO "));
  Serial.println(ADCO);
  Serial.print(F("OPTARIF "));
  Serial.println(OPTARIF);
  Serial.print(F("ISOUSC "));
  Serial.println(ISOUSC);
  Serial.print(F("HCHC "));
  Serial.println(HCHC);
  Serial.print(F("BASE "));
  Serial.println(BASE);
  Serial.print(F("HCHP "));
  Serial.println(HCHP);
  Serial.print(F("PTEC "));
  Serial.println(PTEC);
  Serial.print(F("IINST "));
  Serial.println(IINST);
  Serial.print(F("IMAX "));
  Serial.println(IMAX);
  Serial.print(F("PAPP "));
  Serial.println(PAPP);
  Serial.print(F("HHPHC "));
  Serial.println(HHPHC);
  Serial.print(F("MOTDETAT "));
  Serial.println(MOTDETAT);
}



void encodeur_OWL_CM180()
{

  //Le compteur de production ne positionne ni PTEC ni HCHC ni HCHP, mais alimente BASE
  if (HCHC == 0 && HCHP == 0){PTEC="HPP.";}


  OregonMessageBuffer[0] = 0x62; // imposé
  OregonMessageBuffer[1] = 0x80; // GH   G= non décodé par RFXCOM,  H = Count

  if (PTEC.substring(1, 2) == "P")
  {
    //OregonMessageBuffer[2] =0x3C; // IJ  ID compteur : "L IJ 2" soit (L & 1110 )*16*16*16+I*16*16+J*16+2
    // Si prod 3C, si heure creuse compteur 3D, si HP compteur 3E
    HCP = (BASE * 223666LL) / 1000LL;
    OregonMessageBuffer[2] = 0x3C;
  }
  else
  {
    if (PTEC.substring(1, 2) == "C")
    {
      HCP = (HCHC * 223666LL) / 1000LL;
      OregonMessageBuffer[2] = 0x3D;
    }
    else
    {
      HCP = (HCHP * 223666LL) / 1000LL;
      OregonMessageBuffer[2] = 0x3E;
    }
  }


  //OregonMessageBuffer[3] =0xE1; // KL  K sert pour puissance instantanée, L sert pour identifiant compteur
  PAPP_arrondi=long(long(PAPP)*497L/500L/16L);

  // améliore un peu la précision de la puissance apparente encodée (le CM180 transmet la PAPP * 497/500/16)
  if ((float(PAPP)*497/500/16-PAPP_arrondi)>0.5)
  {
    ++PAPP_arrondi;
  }

  OregonMessageBuffer[3]=(PAPP_arrondi&0x0F)<<4;
  
  //OregonMessageBuffer[4] =0x00; // MN  puissance instantanÃ©e = (P MN K)*16 soit : (P*16*16*16 + M*16*16 +N*16+K)*16*500/497. attention RFXCOM corrige cette valeur en multipliant par 16 puis 500/497.
  OregonMessageBuffer[4]=(PAPP_arrondi>>4)&0xFF;
  
  //OregonMessageBuffer[5] =0xCD; // OP  Total conso :  YZ WX UV ST QR O : Y*16^10 + Z*16^9..R*16 + O
  OregonMessageBuffer[5] =((PAPP_arrondi>>12)&0X0F)+((HCP&0x0F)<<4);
  
  //OregonMessageBuffer[6] =0x97; // QR sert total conso
  OregonMessageBuffer[6] =(HCP>>4)&0xFF;
  
  //OregonMessageBuffer[7] =0xCE; // ST sert total conso
  OregonMessageBuffer[7] =(HCP>>12)&0xFF; // ST sert total conso
  
  //OregonMessageBuffer[8] =0x12; // UV sert total conso
  OregonMessageBuffer[8] =(HCP>>20)&0xFF; // UV sert total conso
  
  //OregonMessageBuffer[9] =0x00; // WX sert total conso
  OregonMessageBuffer[9] =(HCP>>28)&0xFF; 
  
  //OregonMessageBuffer[10] =0x00; //YZ sert total conso
  OregonMessageBuffer[10] =(HCP>>36)&0xFF; 
  
  
    chksum_CM180= 0;
    for (byte i=0; i<11; i++) 
    {
      chksum_CM180 += long(OregonMessageBuffer[i]&0x0F) + long(OregonMessageBuffer[i]>>4) ;
    }
    
  chksum_CM180 -=2; // = =b*16^2 + d*16+ a ou [b d a]
  /*
  Serial.print(F(" chksum_CM180 HEX :"));
  Serial.println(chksum_CM180,HEX);
  */
  
  //OregonMessageBuffer[11] =0xD0; //ab sert CHECKSUM  somme(nibbles ci-dessus)=b*16^2 + d*16+ a + 2
  OregonMessageBuffer[11] =((chksum_CM180&0x0F)<<4) + ((chksum_CM180>>8)&0x0F);
  
  //OregonMessageBuffer[12] =0xF6; //cd  d sert checksum, a non dÃ©codÃ© par RFXCOM
  OregonMessageBuffer[12] =(int(chksum_CM180>>4)&0x0F);  //C = 0 mais inutilisÃ©
  
 /*
   for (byte i = 0; i <=sizeof(OregonMessageBuffer); ++i)   {     
     Serial.print(OregonMessageBuffer[i] >> 4, HEX);
     Serial.println(OregonMessageBuffer[i] & 0x0F, HEX);
    }
*/
  }


boolean Debug()
{
  char bufferTeleinfo[21] = "";

  //test emetteur - Debug
  strcpy(&bufferTeleinfo[0], "ADCO 524563565245");
  handleBuffer(bufferTeleinfo, 1);
  strcpy(&bufferTeleinfo[0], "OPTARIF HC..");
  handleBuffer(bufferTeleinfo, 2);
  strcpy(&bufferTeleinfo[0], "ISOUSC 20");
  handleBuffer(bufferTeleinfo, 3);
  strcpy(&bufferTeleinfo[0], "HCHC 001065963");
  handleBuffer(bufferTeleinfo, 4);
  strcpy(&bufferTeleinfo[0], "HCHP 001521211");
  handleBuffer(bufferTeleinfo, 5);
  strcpy(&bufferTeleinfo[0], "PTEC HC..");
  handleBuffer(bufferTeleinfo, 6);
  strcpy(&bufferTeleinfo[0], "HHPHC E");
  handleBuffer(bufferTeleinfo, 7);
  strcpy(&bufferTeleinfo[0], "IINST 001");
  handleBuffer(bufferTeleinfo, 8);
  strcpy(&bufferTeleinfo[0], "IMAX 008");
  handleBuffer(bufferTeleinfo, 9);
  //strcpy(&bufferTeleinfo[0],"PMAX 0 6030");
  //handleBuffer(bufferTeleinfo, 9);
  strcpy(&bufferTeleinfo[0], "PAPP 01250");
  handleBuffer(bufferTeleinfo, 10);
  strcpy(&bufferTeleinfo[0], "MOTDETAT 000000");
  handleBuffer(bufferTeleinfo, 11);
  //strcpy(&bufferTeleinfo[0],"PPOT 00");
  //handleBuffer(bufferTeleinfo, 13);
  displayTeleInfo();

  return true;
}

//************************************************************************************

void setup() {
  Serial.begin(115200);   // pour la console, enlever les barres de commentaires ci dessous pour displayTeleInfo()
  TeleInfo(version);
  pinMode(TX_PIN, OUTPUT);    //emetteur 433 MHZ
  pinMode(VCC_PIN_TX,OUTPUT);
  pinMode(VCC_PIN_PROD,OUTPUT);
  pinMode(VCC_PIN_CONSO,OUTPUT);
  analogWrite(VCC_PIN_TX,255);
  analogWrite(VCC_PIN_PROD,0);
  analogWrite(VCC_PIN_CONSO,0);
  ethernetIsOK=true;
}

void loop() {

  //teleInfoReceived = Debug();
  //fin test debug 
  teleInfoReceived=readTeleInfo(ethernetIsOK);
  if (teleInfoReceived)
  {
    encodeur_OWL_CM180();
    Conso->end(); //NECESSAIRE !! arrête les interruptions de softwareserial (lecture du port téléinfo) pour émission des trames OWL
    Prod->end(); //NECESSAIRE !! arrête les interruptions de softwareserial (lecture du port téléinfo) pour émission des trames OWL
    analogWrite(VCC_PIN_PROD,255);
    analogWrite(VCC_PIN_CONSO,255);
    analogWrite(VCC_PIN_TX,0);
    sendOregon(OregonMessageBuffer, sizeof(OregonMessageBuffer));    // Send the Message over RF
    analogWrite(VCC_PIN_PROD,0);
    analogWrite(VCC_PIN_CONSO,0);
    analogWrite(VCC_PIN_TX,255);    
    Conso->begin(1200);  //NECESSAIRE !! relance les interuptions pour la lecture du port téléinfo Conso
    Prod->begin(1200);  //NECESSAIRE !! relance les interuptions pour la lecture du port téléinfo Prod
    displayTeleInfo();  // console pour voir les trames téléinfo
    // ajout d'un delais de 12s apres chaque trame envoyés pour éviter d'envoyer
    // en permanence des informations à domoticz et de créer des interférances
    delay(150000);
    //on change de compteur
    //if (ethernetIsOK == true){ethernetIsOK=false;}else{ethernetIsOK=true;}
  }
}
