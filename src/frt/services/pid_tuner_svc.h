#ifndef __PID_TUNER_SVC_H__
#define __PID_TUNER_SVC_H__

#ifdef ESP32
#include <Arduino.h>
#include <frt/frt.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <frt/services/output_control_svc.h>

#define RECORD_PID_VALUES "pid_values"
#define RECORD_TEMPERATURE "temperature"

struct PIDValues
{
    float setpoint;
    float p;
    float i;
    float d;
};

union Float
{
    float f;
    uint8_t bytes[4];
};

namespace frt
{
    class PIDTunerServiceTCP : public frt::Task<PIDTunerServiceTCP, 4096>
    {
    public:
        PIDTunerServiceTCP(const char *ssid, const char *password, uint16_t port = 1234);
        virtual ~PIDTunerServiceTCP();
        bool run() override;

    private:
        Publisher<OutputPower> *_output_pub;
        Subscriber<msgs::Temperature> *_input_sub;
        AsyncServer *_server;
        AsyncClient *_client;
    };
}

#endif
#endif // __PID_TUNER_SVC_H__