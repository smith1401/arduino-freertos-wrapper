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
#endif