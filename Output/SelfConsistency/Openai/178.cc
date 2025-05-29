#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation parameters
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1024;   // bytes
    uint32_t numPackets = 50;
    double interval = 0.1;        // seconds

    // Enable logging for debugging if needed
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Select 802.11n standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    // Set up Ad-hoc MAC
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Configure physical layer (MCS, TxPower, etc.)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));
    // Install devices
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility Models: Place two nodes apart from each other
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(50.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP server on node 1
    uint16_t serverPort = 4000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Set up UDP client on node 0
    UdpClientHelper udpClient(interfaces.GetAddress(1), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    // NetAnim setup
    AnimationInterface anim("wifi-adhoc-80211n.xml");
    // Optional: Set node description and colors for clarity
    anim.UpdateNodeDescription(0, "Node0");
    anim.UpdateNodeColor(0, 0, 255, 0); // green
    anim.UpdateNodeDescription(1, "Node1");
    anim.UpdateNodeColor(1, 255, 0, 0); // red

    // Enable packet-level animation
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}