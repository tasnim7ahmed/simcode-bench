#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 5;
    double simulationTime = 30.0; // seconds
    double distance = 50.0;

    // Enable packet metadata for analysis
    Packet::EnablePrinting();
    Packet::EnableChecking();

    // Enable logging for AODV
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Set up Wifi PHY and MAC (AdHoc)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                             "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));

    mobility.Install(nodes);

    // Set up mobility trace output
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> osw = ascii.CreateFileStream("mobility-trace.txt");
    mobility.EnableAsciiAll(osw);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP traffic flow from node 0 to node 4
    uint16_t port = 5000;
    // Create UDP server on node 4
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // UDP client on node 0 targeting node 4
    UdpClientHelper udpClient(interfaces.GetAddress(4), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable packet reception tracing
    wifiPhy.EnablePcapAll("aodv-manet");

    // NetAnim trace
    AnimationInterface anim("aodv-manet.xml");

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Packet delivery ratio calculation
    uint32_t receivedPackets = udpServer.GetServer()->GetReceived();
    uint32_t sentPackets = DynamicCast<UdpClient>(clientApps.Get(0))->GetObject<UdpClient>()->GetSent();

    std::cout << "Total packets sent: " << sentPackets << std::endl;
    std::cout << "Total packets received: " << receivedPackets << std::endl;
    if (sentPackets > 0)
    {
        double pdr = 100.0 * receivedPackets / sentPackets;
        std::cout << "Packet Delivery Ratio: " << pdr << "%" << std::endl;
    }
    else
    {
        std::cout << "No packets sent!" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}