#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    uint32_t numNodes = 10;
    double areaSize = 500.0;  // in meters
    double maxSpeed = 5.0;    // m/s
    double pauseTime = 2.0;   // seconds
    uint16_t serverPort = 4000;
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 20;
    TimeValue interval(Seconds(1.0));
    Time simDuration = Seconds(20);

    // Enable AODV logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(maxSpeed) + "]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(pauseTime) + "]"),
                              "PositionAllocator", StringValue("ns3::RandomRectanglePositionAllocator"));
    mobility.Install(nodes);

    // Configure WiFi (Ad Hoc mode)
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    wifi.SetStandard(WIFI_STANDARD_80211b);
    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Install Internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAssigner;
    ipAssigner.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipAssigner.Assign(wifiDevices);

    // Setup UDP Server on node 0
    UdpEchoServerHelper echoServer(serverPort);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(simDuration);

    // Setup UDP Client on node 1
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient.SetAttribute("Interval", interval);
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = echoClient.Install(nodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(simDuration);

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("manet-aodv");

    // Run simulation
    Simulator::Stop(simDuration);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}