#ifndef __PID_TUNER_SVC_H__
#define __PID_TUNER_SVC_H__

#ifdef ESP32
#include <Arduino.h>
#include <frt/frt.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <sTune.h>
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

    // class PIDAutoTunerService : public frt::Task<PIDAutoTunerService>
    // {
    // public:
    //     PIDAutoTunerService();
    //     virtual ~PIDAutoTunerService();
    //     bool run() override;

    // private:
    //     Publisher<msgs::PID> *_pid_pub;
    //     Subscriber<msgs::Temperature> *_input_sub;
    //     sTune *_tuner;

    //     // user settings
    //     uint32_t settleTimeSec = 10;
    //     uint32_t testTimeSec = 500; // runPid interval = testTimeSec / samples
    //     const uint16_t samples = 500;
    //     const float inputSpan = 200;
    //     const float outputSpan = 1000;
    //     float outputStart = 0;
    //     float outputStep = 50;
    //     float tempLimit = 150;
    //     uint8_t debounce = 1;

    //     float Input, Output, Setpoint = 80, Kp, Ki, Kd;
    // };
}

#endif
#endif // __PID_TUNER_SVC_H__