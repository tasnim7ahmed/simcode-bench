#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvWirelessAdHoc");

int main(int argc, char *argv[])
{
    LogComponentEnable("AodvWirelessAdHoc", LOG_LEVEL_INFO);
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_DEBUG); // To see AODV routing protocol messages
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Command line arguments
    uint32_t numNodes = 5;
    double simulationTime = 10.0; // seconds
    double packetInterval = 1.0; // seconds
    uint32_t packetCount = 10;
    uint32_t packetSize = 1024; // bytes

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the simulation", numNodes);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.Parse(argc, argv);

    // 1. Create Nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // 2. Set up Mobility Model (Random Walk)
    MobilityHelper mobility;
    mobility.SetPositionAllocator(
        "ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
    );
    mobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]")
    );
    mobility.Install(nodes);

    // 3. Set up Wireless Channel and Physical Layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ); // Using 802.11n for better performance

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(YansWifiChannelHelper::Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(20.0)); // dBm
    wifiPhy.Set("TxPowerEnd", DoubleValue(20.0));   // dBm
    wifiPhy.Set("TxPowerLevels", UintegerValue(1));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-82)); // dBm
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-85));       // dBm

    // 4. Set up MAC Layer (Ad-hoc)
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    // 5. Install WiFi Devices
    NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 6. Install AODV Routing Protocol
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internet.Install(nodes);

    // 7. Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 8. Set up UDP Traffic (UdpEchoClient and UdpEchoServer)
    // Server on the last node
    UdpEchoServerHelper echoServer(9); // Port 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(numNodes - 1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Client on the first node, sending to the server
    UdpEchoClientHelper echoClient(interfaces.GetAddress(numNodes - 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start after 1 second to allow routes to form
    clientApps.Stop(Seconds(simulationTime));

    // 9. Enable PCAP Tracing
    // Trace MAC layer activity
    wifiPhy.EnablePcapAll("aodv-wireless-ad-hoc");

    // Trace IP layer activity for AODV
    aodv.EnablePcapAll("aodv-wireless-ad-hoc-aodv");

    // 10. Enable NetAnim Tracing (optional, for visualization)
    AnimationInterface anim("aodv-wireless-ad-hoc.xml");
    anim.SetConstantPosition(nodes.Get(0), 10, 50); // Set fixed position for node 0 for illustration in NetAnim
    anim.SetConstantPosition(nodes.Get(numNodes - 1), 90, 50); // Set fixed position for last node
    for (uint32_t i = 1; i < numNodes - 1; ++i) {
        // Other nodes use random walk, NetAnim will show their movement
    }


    // 11. Run Simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}