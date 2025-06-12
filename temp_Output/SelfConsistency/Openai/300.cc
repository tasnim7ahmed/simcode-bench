#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigbeeWsnExample");

int main(int argc, char *argv[])
{
    // Set up logging (optional, for debugging)
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Set simulation parameters
    uint32_t numNodes = 10;
    double areaSize = 100.0; // meters
    double simTime = 10.0;   // seconds
    double packetInterval = 0.5; // seconds
    uint32_t packetSize = 128;   // bytes

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Set up mobility: RandomWalk2dMobilityModel in a 100x100 area
    MobilityHelper mobility;
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, areaSize, 0.0, areaSize)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]") // 1 m/s
    );
    mobility.SetPositionAllocator(
        "ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
    );
    mobility.Install(nodes);

    // Install ZigBee stack (IEEE 802.15.4 + ZigBee PRO)
    ZigbeeHelper zigbee;
    // Set device type: node 0 = coordinator, others = end devices
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        if (i == 0)
        {
            zigbee.SetType("ns3::ZigbeeDevice",
                           "MacType", EnumValue(ZIGBEE_MAC_TYPE_COORDINATOR),
                           "MacMode", EnumValue(ZIGBEE_PRO));
        }
        else
        {
            zigbee.SetType("ns3::ZigbeeDevice",
                           "MacType", EnumValue(ZIGBEE_MAC_TYPE_END_DEVICE),
                           "MacMode", EnumValue(ZIGBEE_PRO));
        }
    }
    NetDeviceContainer devices = zigbee.Install(nodes);

    // Set up internet stack (UDP/IP) over ZigBee
    InternetStackHelper internet;
    internet.Install(nodes);

    ZigbeeIpv6InterfaceHelper ipv6helper;
    Ipv6InterfaceContainer interfaces = ipv6helper.Assign(devices);
    // Use link-local addresses unless global needed

    // Install UDP server on coordinator (node 0)
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP client on each end device (nodes 1..9)
    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        // Use node 0's link-local IPv6 address
        Ipv6Address destAddr = interfaces.GetAddress(0, 1); // 0=coordinator, 1=link-local scope
        UdpClientHelper udpClient(destAddr, udpPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(uint32_t(simTime / packetInterval)));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer app = udpClient.Install(nodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simTime));
        clientApps.Add(app);
    }

    // Enable PCAP tracing for ZigBee
    zigbee.EnablePcapAll("zigbee-wsn");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}