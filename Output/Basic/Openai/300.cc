#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/zigbee-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigBeeWSN");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("ZigBeeWSN", LOG_LEVEL_INFO);

    uint32_t numNodes = 10;
    double simTime = 10.0;
    uint32_t packetSize = 128;
    Time interPacketInterval = Seconds(0.5);

    NodeContainer nodes;
    nodes.Create(numNodes);

    ZigbeeHelper zigbee;
    zigbee.SetChannel("ns3::ZigbeeChannel");
    zigbee.SetMac("ns3::ZigbeeMac");

    NetDeviceContainer devs = zigbee.Install(nodes);

    // Set ZigBee PRO Mode
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<ZigbeeNetDevice> zn = devs.Get(i)->GetObject<ZigbeeNetDevice>();
        zn->SetProMode(true);
    }

    // Coordinator: node 0, others: end devices
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<ZigbeeNetDevice> zn = devs.Get(i)->GetObject<ZigbeeNetDevice>();
        if (i == 0)
        {
            zn->GetMac()->SetType(ZIGBEE_MAC_COORDINATOR);
        }
        else
        {
            zn->GetMac()->SetType(ZIGBEE_MAC_END_DEVICE);
        }
    }

    // Random walk mobility in 100m x 100m area
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Distance", DoubleValue(5),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.Install(nodes);

    // Internet stack and IPv6 (as ZigBee typically uses 6LoWPAN/IPv6)
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifaces = ipv6.Assign(devs);

    // Coordinator address (node 0)
    Ipv6Address coordinatorAddr = ifaces.GetAddress(0, 1);

    // Setup UDP server on coordinator
    uint16_t udpPort = 4000;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Setup UDP client on each end device
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        UdpClientHelper client(coordinatorAddr, udpPort);
        client.SetAttribute("MaxPackets", UintegerValue((uint32_t)(simTime / interPacketInterval.GetSeconds())));
        client.SetAttribute("Interval", TimeValue(interPacketInterval));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(0.5));
        clientApp.Stop(Seconds(simTime));
    }

    // Enable ZigBee PCAP tracing
    zigbee.EnablePcapAll("zigbee-wsn");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}