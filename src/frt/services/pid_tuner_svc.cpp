#include "pid_tuner_svc.h"

#ifdef ESP32

using namespace frt;

PIDTunerServiceTCP::PIDTunerServiceTCP(const char *ssid, const char *password, uint16_t port) : _client(nullptr)
{
    // Init publisher and subscribers
    _output_pub = frt::pubsub::advertise<OutputPower>(RECORD_OUTPUT_POWER);
    _input_sub = frt::pubsub::subscribe<msgs::Temperature>(RECORD_TEMPERATURE);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("src-pid-tuner");
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        FRT_LOG_DEBUG("WiFi Failed");
        noInterrupts();
        while (1)
        {
            delay(1000);
        }
    }

    // start DNS service
    MDNS.addService("http", "tcp", 80);

    _server = new AsyncServer(WiFi.localIP(), port);
    _server->onClient([this](void *arg, AsyncClient *client)
                      {
        FRT_LOG_DEBUG("Client connected to PID tuner");
        _client = client;
        _client->onData([this](void *arg, AsyncClient *client, void *data, size_t len)
        {
            OutputPower output;
            memcpy(&output.power, data, len);
            // FRT_LOG_DEBUG("New output power: %d", output.power);
            _output_pub->publish(output);

            // PIDValues val;
            // memcpy(&val, data, len);
            // msgs::PID msg;
            // msg.timestamp = xTaskGetTickCount();
            // msg.setpoint = val.setpoint;
            // msg.p = val.p;
            // msg.i = val.i;
            // msg.d = val.d;

            // FRT_LOG_DEBUG("Setpoint: %.1f\tP: %.1f\tI: %.1f\tD: %.1f", msg.setpoint, msg.p, msg.i, msg.d);
            // _pid_pub->publish(msg);
        }, arg); },
                      this);

    _server->begin();

    FRT_LOG_INFO("PID Tuner Service started with IP %s on port %d", WiFi.localIP().toString().c_str(), port);
}

PIDTunerServiceTCP::~PIDTunerServiceTCP()
{
}

bool PIDTunerServiceTCP::run()
{
    msgs::Temperature input;

    if (_input_sub->receive(input) && _client != nullptr && _client->connected())
    {
        Float f = { input.temperature };
        _client->write((const char*)&f.bytes, 4);
    }

    return true;
}

// PIDAutoTunerService::PIDAutoTunerService()
// {
//     // Init publisher and subscribers
//     _pid_pub = frt::pubsub::advertise<msgs::PID>(RECORD_PID_VALUES);
//     _input_sub = frt::pubsub::subscribe<msgs::Temperature>(RECORD_TEMPERATURE);

//     _tuner = new sTune(&Input, &Output, _tuner->ZN_PID, _tuner->directIP, _tuner->printOFF);

//     Output = 0;
//     _tuner->Configure(inputSpan, outputSpan, outputStart, outputStep, testTimeSec, settleTimeSec, samples);
//     _tuner->SetEmergencyStop(tempLimit);
// }

// PIDAutoTunerService::~PIDAutoTunerService()
// {
// }

// bool PIDAutoTunerService::run()
// {
//     float optimumOutput = _tuner->softPwm(PIN_TRIAC, Input, Output, Setpoint, outputSpan, debounce);

//      msgs::Temperature input;

//     if (!_input_sub->receive(input))
//     {
//         return true;
//     }

//     switch (_tuner->Run())
//     {
//     case _tuner->sample: // active once per sample during test
//         Input = input.temperature;
//         _tuner->plotter(Input, Output, Setpoint, 0.5f, 3); // output scale 0.5, plot every 3rd sample
//         break;

//     case _tuner->tunings:                      // active just once when sTune is done
//         _tuner->GetAutoTunings(&Kp, &Ki, &Kd); // sketch variables updated by sTune
//         // myPID.SetOutputLimits(0, outputSpan * 0.1);
//         // myPID.SetSampleTimeUs((outputSpan - 1) * 1000);
//         debounce = 0; // ssr mode
//         Output = outputStep;
//         // myPID.SetMode(myPID.Control::automatic); // the PID is turned on
//         // myPID.SetProportionalMode(myPID.pMode::pOnMeas);
//         // myPID.SetAntiWindupMode(myPID.iAwMode::iAwClamp);
//         // myPID.SetTunings(Kp, Ki, Kd); // update PID with the new tunings
//         // TODO: set P,I and D values with publisher
//         break;

//     case _tuner->runPid: // active once per sample after tunings
//         Input = input.temperature;
//         // myPID.Compute();
//         _tuner->plotter(Input, optimumOutput, Setpoint, 0.5f, 3);
//         break;
//     }

//     return true;
// }

#endif