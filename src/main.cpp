/**
    Copyright:  zirexix 2016-2017

    This program is distributed
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

/*
    This program should run as root.

    Every time a characterisitc/service is chnaged,
    turn off and on on mobile the BT to clear the cached LE's.

    http://plugable.com/2014/06/23/plugable-usb-bluetooth-adapter-solving-hfphsp-profile-issues-on-linux
    Newer Kernel Versions (3.16 and later)
    wget https://s3.amazonaws.com/plugable/bin/fw-0a5c_21e8.hcd
    sudo mkdir /lib/firmware/brcm
    sudo mv fw-0a5c_21e8.hcd /lib/firmware/brcm/BCM20702A0-0a5c-21e8.hcd


*/
// #define XECHO
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <termios.h>
#include <stropts.h>
#include <libbunget.h>
#include "crypto.h"

using namespace std;

bool __alive = true;

/****************************************************************************************
 * intrerrupt the demo in a orthodox way
*/
int _kbhit() {
    static const int STDIN = 0;
    static bool initialized = false;

    if (! initialized) {
        // Use termios to turn off line buffering
        termios term;
        tcgetattr(STDIN, &term);
        term.c_lflag &= ~ICANON;
        tcsetattr(STDIN, TCSANOW, &term);
        setbuf(stdin, NULL);
        initialized = true;
    }

    int bytesWaiting;
    ioctl(STDIN, 0x541B, &bytesWaiting);
    return bytesWaiting;
}

/****************************************************************************************
 * user procedure to intercept btle events  on****()
*/
class my_proc : public ISrvProc
{
public:
    my_proc();
    Icryptos* get_crypto(){return &_crypt;};
    bool initHciDevice(int devid, const char* name);
    void onServicesDiscovered(std::vector<IHandler*>& els);
    void onReadRequest(IHandler* pc);
    int  onSubscribesNotify(IHandler* pc, bool b);
    void onIndicate(IHandler* pc);
    void onWriteRequest(IHandler* pc);
    void onWriteDescriptor(IHandler* pc, IHandler* pd);
    void onAdvertized(bool onoff);
    void onDeviceStatus(bool onoff);
    void onStatus(const HciDev* connected);
    bool onSpin(IServer* ps);

private:
    void        _send_value(IHandler* pc);

public:
    char        _some[20];
    bool        _subscribed;
    IHandler*   StatChr;       // r
    IHandler*   SsidChr;       // w
    IHandler*   PskChr;       // w
    IHandler*   FpaChr;       // R
    IHandler*   FpbChr;       // R
    IHandler*   NonChr;       // R
private:    
    cryptos     _crypt;         // MANDATORY, detached form lib, Use it on your own GNU
};

/****************************************************************************************
*/
#define UID_STAT    0xfc0a
#define UID_SSID    0xfc0b
#define UID_PSKC    0xfc0c
#define UID_FPAC    0xfc0d
#define UID_FPBC    0xfc0e
#define UID_NONC    0xfc0f

/****************************************************************************************
 * demo main program
*/
int main(int n, char* v[])
{
    std::cout << LIBBUNGET_VERSION_STRING << "\n";
    if(n==1)
    {
        std::cout << "sudo bunget hcidev#, pass device id as first argument!\n";
        return -1;
    }
    if(getuid()!=0)
    {
        std::cout << "sudo bunget hcidev#, run under sudo credentials!\n";
        return -1;
    }
    
    BtCtx*      ctx = BtCtx::instance();                // BT context
    my_proc     procedure;                              // this procedure
    int dev = ::atoi(v[1]);

    
    try{
        IServer*    BS =  ctx->new_server(&procedure, dev, "Pipe", 16);

#if 0   // not tested !!!
        //BS->set_name("advname"); // this is the bt name.
        //99999999-9999-9999-9999-999999999999
        BS->adv_beacon("11111111-1111-1111-1111-111111111111", 1, 10, -10, 0x004C, (const uint8_t*)"todo", 7);
#endif // 0

        IService*   ps = BS->add_service(0x919e,"Pipe");
        
        procedure.StatChr = ps->add_charact(UID_STAT,PROPERTY_READ|PROPERTY_NOTIFY,
                                 0,
                                 FORMAT_RAW, 4);
        procedure.SsidChr = ps->add_charact(UID_SSID,PROPERTY_WRITE|PROPERTY_INDICATE,
                                 0,
                                 FORMAT_RAW, 16);
        procedure.PskChr = ps->add_charact(UID_STAT,PROPERTY_WRITE|PROPERTY_INDICATE,
                                 0,
                                 FORMAT_RAW, 16);
        procedure.FpaChr = ps->add_charact(UID_FPAC,PROPERTY_READ|PROPERTY_NOTIFY,
                                 0,
                                 FORMAT_RAW, 16);
        procedure.FpbChr = ps->add_charact(UID_FPBC,PROPERTY_READ|PROPERTY_NOTIFY,
                                 0,
                                 FORMAT_RAW, 16);
        procedure.NonChr = ps->add_charact(UID_NONC,PROPERTY_READ|PROPERTY_NOTIFY,
                                 0,
                                 FORMAT_RAW, 16);
        BS->advertise(true);

        BS->run();
        BS->stop();
 
    }
    catch(bunget::hexecption& ex)
    {

        ERROR (ex.report());
    }

    return 0;
}

/****************************************************************************************
*/
my_proc::my_proc()
{
    _subscribed=false;
}

/****************************************************************************************
 * add your console hciconfig preambul to setup hci before BTLE is starting
*/
bool my_proc::initHciDevice(int devid, const char* devn)
{
  
    char name[128];
    // system("service bluetoothd stop");
    // system("service bluetooth stop");
    // system("sudo systemctl stop bluetooth");
    // system("rfkill unblock bluetooth");
    ::sprintf(name,"hciconfig hci%d down", devid);
    system(name);
    ::sleep(2);
    ::sprintf(name,"hciconfig hci%d up", devid);
    system(name);
/*
    ::sprintf(name,"hciconfig hci%d sspmode 0", devid);
    system(name);
    ::sprintf(name,"hciconfig hci%d nosecmgr", devid);
    system(name);
    ::sprintf(name,"hciconfig hci%d noencrypt", devid);
    system(name);
*/ 
    ::sprintf(name,"hciconfig hci%d noauth", devid);
    system(name);
    ::sprintf(name,"hciconfig hci%d noleadv", devid);
    system(name);
    ::sprintf(name,"hciconfig hci%d noscan", devid);
    system(name);
    ::sprintf(name,"hciconfig hci%d name  %s", devid, devn);
    system(name);
/*
    ::sprintf(name,"hciconfig hci%d piscan", devid);
    system(name);
    ::sprintf(name,"hciconfig hci%d leadv", devid);
    system(name);
*/

    printf("%s", "done dirty work\n");
    
    return true;
}

/****************************************************************************************
*/
bool my_proc::onSpin(IServer* ps)
{
    static int inawhile=0;

    if(_kbhit()){
        if(getchar()=='q')
        return false;
    }

    // notification
    if(inawhile++%100==0)
    {
        if(_subscribed)
        {
            _send_value(StatChr);
        }
    }
    return true;
}
/**

}


*/

/****************************************************************************************
*/
void my_proc::onServicesDiscovered(std::vector<IHandler*>& els)
{
    TRACE("my_proc event: onServicesDiscovered");
}

/****************************************************************************************
*/
/// remote readsd pc characteritcis
void my_proc::onReadRequest(IHandler* pc)
{
    TRACE("my_proc event:  onReadRequest:" <<  std::hex<< pc->get_16uid() << std::dec);
    _send_value(pc);
}

/****************************************************************************************
*/
int my_proc::onSubscribesNotify(IHandler* pc, bool b)
{
    TRACE("my_proc event: onSubscribesNotify:" << std::hex<< pc->get_16uid() << "="<<(int)b<< std::dec);
    _subscribed = b;
    return 0 ;
}

/****************************************************************************************
*/
void my_proc::onIndicate(IHandler* pc)
{
    TRACE("my_proc event:  onIndicate:" <<  std::hex<< pc->get_16uid() << std::dec);
    _send_value(pc);
}

/****************************************************************************************
*/
void my_proc::onWriteRequest(IHandler* pc)
{
    TRACE("my_proc event:  onWriteRequest:" <<  std::hex<< pc->get_16uid() << std::dec);
    std::string     ret;
    const uint8_t*  value = pc->get_value();
    char            by[4];
    int             i=0;

    for(;i<pc->get_length();i++)
    {
        ::sprintf(by,"%02X:",value[i]);
        ret.append(by);
    }
    TRACE("Remote data:" << ret);
}

/****************************************************************************************
*/
//descriptor chnaged of the charact
void my_proc::onWriteDescriptor(IHandler* pc, IHandler* pd)
{
    TRACE("my_proc event:  onWriteDescriptor:" << int(*((int*)(pd->get_value()))));
}

/****************************************************************************************
*/
void my_proc::onAdvertized(bool onoff)
{
    TRACE("my_proc event:  onAdvertized:" << onoff);
}

/****************************************************************************************
*/
void my_proc::onDeviceStatus(bool onoff)
{
    TRACE("my_proc event:  onDeviceStatus:" << onoff);
    if(onoff==false)
    {
        _subscribed = false;
    }
}

/****************************************************************************************
*/
void my_proc::onStatus(const HciDev* device)
{
    if(device == 0)
    {
        _subscribed = false;
        TRACE("my_proc event: onStatus: disconnected");
    }
    else
    {
        TRACE("accepted connection: " << device->_mac <<","<< device->_name);
    }
}


/****************************************************************************************
*/
void my_proc::_send_value(IHandler* pc)
{
    uint16_t uid = pc->get_16uid();
    switch(uid)
    {
        case  UID_STAT:
            {
                const char* t = "off";
                pc->put_value((uint8_t*)t,::strlen(t));
            }
            break;
        case  UID_FPAC:
            {
                const uint8_t t[] = {0xBD,0xE3,0xCC,0x6C,0xA1,0x7A,0x60,0xB0,
                                 0x3C,0x43,0x37,0x36,0x69,0xEE,0x7C,0xA6};
                pc->put_value(t,16);
            }
            break;
        case  UID_FPBC:
            {
                const uint8_t t[] = {0x5D,0x67,0xFE,0xFF,0x72,0x92,0x66,0x49,
                                      0x4C,0x13,0x11,0x8C,0xEC,0xE2,0xC2,0xA3};
                pc->put_value(t,16);
            }
            break;
        case  UID_NONC:
            {
                const uint8_t t[] = {0xD3,0xE5,0x3A,0x6E,0xA7,0x31,0xA9,0xC8,
                                    0xF8,0xC8,0xFF,0xC5,0x2B,0x24,0x7D,0x08};
                pc->put_value(t,16);
            }
            break;
        default:
            break;
    }
}


