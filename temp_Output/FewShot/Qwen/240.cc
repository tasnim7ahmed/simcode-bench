#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UDP applications
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes: 1 sender and 3 receivers
    NodeContainer nodes;
    nodes.Create(4);
    Ptr<Node> sender = nodes.Get(0);
    NodeContainer receivers = NodeContainer(nodes.Get(1), nodes.Get(2), nodes.Get(3));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create Wi-Fi channel and MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("multicast-wifi-network");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configure multicast group address
    Ipv4Address multicastGroup("225.1.2.4");

    // Set up UDP server (PacketSink) on receivers
    uint16_t port = 9;
    PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    ApplicationContainer serverApps;

    for (uint32_t i = 0; i < receivers.GetN(); ++i) {
        serverApps.Add(packetSink.Install(receivers.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client on the sender node
    OnOffHelper clientHelper("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = clientHelper.Install(sender);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up multicast routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting(nodes.Get(i)->GetObject<Ipv4>());
        if (i == 0) {
            // Sender adds a default route
            routing->SetDefaultRoute(interfaces.GetAddress(0), 1);
        } else {
            routing->SetDefaultRoute(interfaces.GetAddress(i), 1);
        }
    }

    // Join multicast group on all receivers
    for (uint32_t i = 0; i < receivers.GetN(); ++i) {
        Ptr<Ipv4Multicast> ipv4Multicast = receivers.Get(i)->GetObject<Ipv4Multicast>();
        NS_ASSERT_MSG(ipv4Multicast, "Ipv4Multicast not installed on receiver node!");
        ipv4Multicast->AddMulticastGroup(Ipv4Address::GetZero(), multicastGroup);
    }

    // Enable PCAP tracing
    AsciiTraceHelper asciiTraceHelper;
    PointToPointHelper pointToPoint;
    CsmaHelper csma;
    WifiMacHelper::EnableAsciiAll(asciiTraceHelper.CreateFileStream("wifi-multicast.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}