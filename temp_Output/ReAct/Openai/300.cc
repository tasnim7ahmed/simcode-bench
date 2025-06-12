#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numNodes = 10;
    double simTime = 10.0;
    double areaSize = 100.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Assign coordinator and end devices
    Ptr<Node> coordinator = nodes.Get(0);

    // ZigBee device and channel configuration
    ZigbeeHelper zigbee;
    Ptr<ZigbeePhyHelper> phy = CreateObject<ZigbeePhyHelper>();
    phy->SetChannel(CreateObject<ZigbeeChannelHelper>()->Create());
    phy->Set("Standard", EnumValue(ZIGBEE_PHY_STANDARD_IEEE802154_PRO)); // ZigBee PRO mode

    ZigbeeMacHelper mac;
    mac.SetType("ns3::ZigbeeMac",
                "PanId", UintegerValue(0x1234),
                "BeaconOrder", UintegerValue(15), // non-beacon enabled
                "SuperframeOrder", UintegerValue(15));

    // Coordinator MAC configuration
    mac.Set("IsPanCoordinator", BooleanValue(true));
    NetDeviceContainer devCoordinator = zigbee.Install(phy, mac, coordinator);

    // End devices MAC configuration
    mac.Set("IsPanCoordinator", BooleanValue(false));
    NetDeviceContainer devEndDevices = zigbee.Install(phy, mac, NodeContainer(nodes.Begin() + 1, nodes.End()));

    // Aggregate all devices
    NetDeviceContainer allDevices;
    allDevices.Add(devCoordinator);
    allDevices.Add(devEndDevices);

    // Mobility configuration (RandomWalk2d within area)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, areaSize, 0.0, areaSize)),
                             "Distance", DoubleValue(10.0),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Direction", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.283185307]") );
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.Install(nodes);

    // Internet and address assignment
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv6AddressHelper address;
    address.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = address.Assign(allDevices);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    // Coordinator: UDP server on port 50000
    uint16_t port = 50000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(coordinator);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // End devices: UDP client
    for (uint32_t i = 1; i < numNodes; ++i)
    {
        UdpClientHelper client(interfaces.GetAddress(0,1), port);
        client.SetAttribute("MaxPackets", UintegerValue((uint32_t)(simTime * 2))); // 2 packets/sec, 10s
        client.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
        client.SetAttribute("PacketSize", UintegerValue(128));
        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0)); // start after 1s
        clientApp.Stop(Seconds(simTime));
    }

    // Enable PCAP tracing
    zigbee.EnablePcapAll("zigbee-wsn");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}