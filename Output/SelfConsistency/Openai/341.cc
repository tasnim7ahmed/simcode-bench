#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure the wireless PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    WifiMacHelper mac;

    // Set it up as an ad hoc network
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set up mobility: RandomWalk2dMobilityModel for both nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Mode", StringValue("Time"),
        "Time", TimeValue(Seconds(2.0)),
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
        "Bounds", RectangleValue(Rectangle(0, 100, 0, 100))
    );
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (10.0),
                                  "MinY", DoubleValue (10.0),
                                  "DeltaX", DoubleValue (20.0),
                                  "DeltaY", DoubleValue (20.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));

    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP server on node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));

    // Set up UDP client on node 0 to send to node 1
    uint32_t maxPackets = 100;
    Time interPacketInterval = Seconds(0.1);
    uint32_t packetSize = 512;

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start after server
    clientApps.Stop(Seconds(20.0));

    // Run simulation
    Simulator::Stop(Seconds(20.0));

    // Uncomment the following line to enable NetAnim visualization
    // AnimationInterface anim("manet2nodes.xml");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}