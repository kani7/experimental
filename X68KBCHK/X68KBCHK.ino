//----------------------------------------------------------------------
// X68000 のキーボードからの送信データを識別する
//----------------------------------------------------------------------
// 2022-07
//----------------------------------------------------------------------
// キーボードコネクタ配線(レセプタクル側)
//
//   7 6 5
//  4     3
//   2   1
//
//----------------------------------------------------------------------
// 端子割り当て状況
//
//  X68kでの名称     Arduino側              備考
//  --------------------------------------------------------------------
//  1:Vcc2 5V(out)  5V                      常駐電源
//  2:MSDATA(in)                            マウスデータ(未配線)
//  3:KEYRxD(in)    A0 SoftwareSerial RX    キーボードからの受信データ
//  4:KEYTxD(out)   A1 SoftwareSerial TX    キーボード宛の送信データ(マウス/LED制御他)
//  5:READY(out)                            キーデータ送出許可/禁止(未配線)
//  6:REMOTE(in)    INT0(2)                 テレビコントロール信号
//  7:GND(--)       GND
//
//-----------------------------------------------------------------------------

//#define MYDEBUG
//#define ShowKeyCode

#define KBD_TX      A1      //KeyBoard TX
#define KBD_RX      A0      //KeyBoard RX
#define KYRMT       2       //KeyBoard TV control, must be interrupt pin
#define TIMEOUT     96000   //TV control signal timeout in microsec, must be more than 35000
#define BLANK       6000    //TV control signal blanking, must be between 2000 and 30000
#define BORDER      1250    //TV control PPM 'L' level duration border between '0' and '1' logic, must be between 750 and 1750


#include <SoftwareSerial.h>

SoftwareSerial KBD(KBD_RX,KBD_TX);

volatile uint8_t tvCode;

void warikomi() {
    static unsigned long prevMicros;
    unsigned long currMicros, timing[13];
    static uint8_t side0, side1;
    uint8_t signalLength = 0;

    delayMicroseconds(300);     //Arduino UNO does not have ability start mesuring within the first 'H' level pulse. WTF
    for(uint8_t i = 0; i < 11; i++) {
        timing[i] = pulseIn(KYRMT, LOW, BLANK);
        if(timing[i] == 0) break;
        signalLength++;
    }

    currMicros = micros();

    union {
        uint16_t pulses;
        struct {
            uint8_t bmSyn : 2;
            uint8_t bmTvcode : 5;
            uint8_t bmFlag : 4;
            uint8_t bmTail : 1;
            uint8_t bmReserved : 4;
        } pulse;
    } XTVCTRL;
    XTVCTRL.pulses = 0xffff;

    if (signalLength != 0) {
        for(uint8_t i = 0; i < signalLength; i++) {
            if(timing[i] > BORDER) {
                XTVCTRL.pulses |= ((uint16_t)0x0001 << i);
            } else {
                XTVCTRL.pulses &= ~((uint16_t)0x0001 << i);
            }
        }
    }
#ifdef MYDEBUG
    if(signalLength != 0) {
        Serial.print(currMicros);
        Serial.print(F(" : Incoming purse(s) : B"));
        Serial.print(XTVCTRL.pulses ,BIN);
        Serial.print(F(" LENGTH ")); Serial.print(signalLength);
        Serial.print(F("d SYN ")); Serial.print(XTVCTRL.pulse.bmSyn , HEX);
        Serial.print(F("h FLAG ")); Serial.print(XTVCTRL.pulse.bmFlag, HEX);
        Serial.print(F("h CODE ")); Serial.print(XTVCTRL.pulse.bmTvcode, HEX);
        Serial.print(F("h"));

        if(signalLength != 11) {
            for(uint8_t i = 0; i < signalLength; i++){
                Serial.println();
                Serial.print(i);
                Serial.print(F(" : "));
                Serial.print(timing[i]);
            }
        }

    }
#endif

    if(side0) side1 = 0x00;

    switch(signalLength) {
        case 9:
        case 10:
            XTVCTRL.pulses = XTVCTRL.pulses << (11 -signalLength);
#ifdef MYDEBUG
            signalLength++;
            Serial.println(F(", SYN broken or missing"));
            Serial.print(currMicros);
            Serial.print(F(" :         it may be : B"));
            Serial.print(XTVCTRL.pulses ,BIN);
            Serial.print(F(" LENGTH ")); Serial.print(signalLength);
            Serial.print(F("d SYN ")); Serial.print(XTVCTRL.pulse.bmSyn , HEX);
            Serial.print(F("h FLAG ")); Serial.print(XTVCTRL.pulse.bmFlag, HEX);
            Serial.print(F("h CODE ")); Serial.print(XTVCTRL.pulse.bmTvcode, HEX);
            Serial.print(F("h"));
#endif
        case 11:
            if ((XTVCTRL.pulse.bmFlag != 0x00) && (XTVCTRL.pulse.bmFlag != 0x0f)) {
#ifdef MYDEBUG
                Serial.print(F(", FLAG broken"));
#endif
                break;
            }
            if (XTVCTRL.pulse.bmSyn == 0x00) {
                if (XTVCTRL.pulse.bmFlag == 0x00) side0 = XTVCTRL.pulse.bmTvcode;
                if (XTVCTRL.pulse.bmFlag == 0x0f) side1 = (~XTVCTRL.pulse.bmTvcode) & 0x1f;
#ifdef MYDEBUG
                Serial.print(F(", pushed"));
#endif
            }
        default:
            break;
    }
#ifdef MYDEBUG
    if(signalLength != 0) Serial.println();
#endif

    if (side1) {
        if (side0 == side1) {
            tvCode = side0;
#ifdef MYDEBUG
            Serial.print(currMicros);
            Serial.print(F(" : TV control code "));
            Serial.print(tvCode, HEX);
            Serial.println(F("h"));
        } else {
            Serial.print(currMicros);
            Serial.print(F(" : code mismatch, "));
            Serial.print(side0, HEX);
            Serial.print(F("h vs "));
            Serial.print(side1, HEX);
            Serial.println(F("h"));
#endif
        }
        side0 = 0x00;
    }
    side1 = 0x00;
    prevMicros = currMicros;
}

void setup() {
    Serial.begin(115200);
    KBD.begin(2400);

    pinMode(KYRMT, INPUT_PULLUP);

    Serial.print(F("HELLO. "));
    Serial.print(F(__DATE__));
    Serial.print(F(" "));
    Serial.println(F(__TIME__));

    KBD.write(0x58);    //TVCTRL disable
    KBD.write(0xff);    //LED off
    uint8_t tvcode = 0x00;
    attachInterrupt(digitalPinToInterrupt(KYRMT), warikomi, RISING);
}

void loop() {
    uint8_t KYDATA;
    KBD.listen();

    if(tvCode != 0x00) {
        Serial.print(F("TV control = 0x"));
        Serial.print(tvCode, HEX);
        Serial.print(F(" : "));
        switch(tvCode) {
            case 0x00: Serial.println(F("invalid or undefined")); break;
            case 0x01: Serial.println(F("volume up")); break;
            case 0x02: Serial.println(F("volume down")); break;
            case 0x03: Serial.println(F("volume normal")); break;
            case 0x04: Serial.println(F("CH call")); break;
            case 0x05: Serial.println(F("CS down ==> TV init")); break;
            case 0x06: Serial.println(F("Mute/Unmute")); break;
            case 0x07: Serial.println(F("CH 16 ==> invalid")); break;
            case 0x08: Serial.println(F("BR up ==> TV/PC")); break;
            case 0x09: Serial.println(F("BR down ==> TV/AUX")); break;
            case 0x0a: Serial.println(F("BR 1/2 ==> contrast normal")); break;
            case 0x0b: Serial.println(F("CH up")); break;
            case 0x0c: Serial.println(F("CH down")); break;
            case 0x0d: Serial.println(F("invalid")); break;
            case 0x0e: Serial.println(F("Power on/off")); break;
            case 0x0f: Serial.println(F("CS 1/2 ==> superimpose on/off, contrast half")); break;
            case 0x10: Serial.println(F("CH 1")); break;
            case 0x11: Serial.println(F("CH 2")); break;
            case 0x12: Serial.println(F("CH 3")); break;
            case 0x13: Serial.println(F("CH 4")); break;
            case 0x14: Serial.println(F("CH 5")); break;
            case 0x15: Serial.println(F("CH 6")); break;
            case 0x16: Serial.println(F("CH 7")); break;
            case 0x17: Serial.println(F("CH 8")); break;
            case 0x18: Serial.println(F("CH 9")); break;
            case 0x19: Serial.println(F("CH 10")); break;
            case 0x1a: Serial.println(F("CH 11")); break;
            case 0x1b: Serial.println(F("CH 12")); break;
            case 0x1c: Serial.println(F("CH 13 ==> TV")); break;
            case 0x1d: Serial.println(F("CH 14 ==> PC")); break;
            case 0x1e: Serial.println(F("CH 15 ==> superimpose on/off, contrast half")); break;
            case 0x1f: Serial.println(F("superimpose on/off, contrast full")); break;
        }
        tvCode = 0;
    }

    if (KBD.available()) {  //データなしは-1が流れてる
        KYDATA = KBD.read();
#ifdef ShowKeyCode
        if(KYDATA > 0x7f) {
            Serial.print(F("KeyUp  "));
            KYDATA = KYDATA - 0x80;
        } else {
            Serial.print(F("KeyDown"));
        }
        Serial.print(F(" code = ")); Serial.print(KYDATA, HEX); Serial.println(F("h"));
#endif
        KBD.write(~KYDATA | 0x80);
        if(KYDATA == 0xff) KBD.write(0xff);
    }
}
