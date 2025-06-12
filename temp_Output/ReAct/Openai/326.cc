#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation parameters
    uint32_t numNodes = 5;
    double simTime = 10.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Wi-Fi PHY and MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper wifiMac;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypoint in 100x100 area
    MobilityHelper mobility;
    Ptr<RandomRectanglePositionAllocator> positionAlloc = CreateObject<RandomRectanglePositionAllocator>();
    positionAlloc->SetX(MakeUniformRandomVariable(0.0, 100.0));
    positionAlloc->SetY(MakeUniformRandomVariable(0.0, 100.0));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(positionAlloc));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(nodes);

    // Install Internet stack (AODV)
    InternetStackHelper internet;
    AodvHelper aodv;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP Server on node 4
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Client on node 0 -> node 4
    UdpClientHelper udpClient(interfaces.GetAddress(4), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable logging (optional)
    //LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    //LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    //LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_INFO);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}