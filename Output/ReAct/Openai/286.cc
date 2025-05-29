#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Wifi Helper - 802.11b
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
        "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=5.0]"),
        "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"),
        "PositionAllocator", PointerValue (
            CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]")
            )
        )
    );
    mobility.Install(nodes);

    // Install Internet stack + AODV routing
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // IP Addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server (Node 4)
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(4));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(20.0));

    // UDP client (Node 0 => Node 4)
    UdpClientHelper client(interfaces.GetAddress(4), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    // Enable PCAP tracing
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("manet-aodv", devices);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}