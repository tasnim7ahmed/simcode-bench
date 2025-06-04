#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocExample");

int main(int argc, char *argv[])
{
    // Simulation time
    double simulationTime = 10.0; // seconds

    // Create two nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Set up Wi-Fi and PHY layers for the nodes
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);  // Use the 802.11n standard (5 GHz)

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    // Use Ad-hoc mode (no infrastructure like APs)
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

    // Install internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set mobility model for the nodes (placing them in fixed positions)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Set up UDP server on Node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Set up UDP client on Node 0 to send packets to Node 1
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.05))); // 20 packets per second
    client.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet

    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    // Set up NetAnim for visualization
    AnimationInterface anim("wifi_adhoc_netanim.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(wifiNodes.Get(0), 1.0, 1.0);  // Client node
    anim.SetConstantPosition(wifiNodes.Get(1), 5.0, 1.0);  // Server node

    // Enable packet metadata in NetAnim
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

