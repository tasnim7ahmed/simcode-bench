#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t numNodes = 5;
    double simTime = 10.0; // seconds
    uint16_t udpPort = 4000;
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 100;
    double interval = 0.1; // seconds

    // 1. Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(numNodes);

    // 2. Create WiFi channel and devices in ad-hoc mode
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, wifiNodes);

    // 3. Set up mobility (Random mobility model)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
        "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
        "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(wifiNodes);

    // 4. Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Set up applications
    // UDP server on last node
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(numNodes - 1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP client on first node -> server
    UdpClientHelper client(interfaces.GetAddress(numNodes - 1), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simTime));

    // 7. Enable pcap tracing (optional)
    // phy.EnablePcapAll("wifi-adhoc-mesh");

    // 8. Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}