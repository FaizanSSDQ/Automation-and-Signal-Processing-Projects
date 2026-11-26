/**
 * @file    sensor_network.ino
 * @brief   ESP-NOW wireless sensor network on ESP32: deep-sleeping sensor nodes
 *          and a gateway with a live dashboard and ThingSpeak upload.
 *
 * The role is chosen at build time (NET_ROLE in config.h, or -DNET_ROLE=1 for
 * the gateway). Nodes report temperature, humidity, pressure, battery and
 * alarm inputs; the gateway validates every frame, tracks signal strength and
 * packet loss per node, and serves http://192.168.4.1 on its own access point.
 *
 * @author  Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)
 */
#include "config.h"

#if NET_ROLE == ROLE_GATEWAY
#include "gateway.h"

void setup()
{
    gateway_setup();
}

void loop()
{
    gateway_loop();
}

#else
#include "node.h"

void setup()
{
    node_run();   /* ends in deep sleep */
}

void loop()
{
}
#endif
