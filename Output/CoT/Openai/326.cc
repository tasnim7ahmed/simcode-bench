#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numNodes = 5;
    double simTime = 10.0;
    double appStart = 2.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(
                                 CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"))));
    mobility.Install(nodes);

    // Wi-Fi (802.11b)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"));

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Internet stack + AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Server (on node 4)
    uint16_t serverPort = 5000;
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApp = server.Install(nodes.Get(4));
    serverApp.Start(Seconds(appStart));
    serverApp.Stop(Seconds(simTime));

    // UDP Client (on node 0)
    UdpClientHelper client(interfaces.GetAddress(4), serverPort);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.03125))); // ~32 packets/sec
    client.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(appStart));
    clientApp.Stop(Seconds(simTime));

    // Enable PCAP tracing
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("manet-80211b", devices);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}