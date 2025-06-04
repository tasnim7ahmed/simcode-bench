#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/zigbee-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigBeeExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("ZigBeeExample", LOG_LEVEL_INFO);

    // Create sensor nodes
    NodeContainer nodes;
    nodes.Create(10);

    // Configure ZigBee communication
    ZigBeeHelper zigbeeHelper;
    zigbeeHelper.SetInterfaceType(ZigBeeHelper::ZIGBEE_PRO);

    // Install ZigBee devices
    NetDeviceContainer devices = zigbeeHelper.Install(nodes);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Create applications (UDP server on the coordinator, client on other nodes)
    uint16_t port = 5000;

    // Set up UDP server (Coordinator)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0)); // Coordinator
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP clients (End Devices)
    for (uint32_t i = 1; i < 10; ++i)
    {
        UdpClientHelper udpClient(interfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(100));
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
        udpClient.SetAttribute("PacketSize", UintegerValue(128));

        ApplicationContainer clientApp = udpClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable tracing
    zigbeeHelper.EnablePcap("zigbee_network", devices);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
