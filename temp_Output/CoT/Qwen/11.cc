#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpCsmaSimulation");

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    std::string addressMode = "Ipv4";

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable logging (true/false)", enableLogging);
    cmd.AddValue("addressMode", "Addressing mode: Ipv4 or Ipv6", addressMode);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        NS_LOG_INFO("Detailed logging enabled for UDP client and server.");
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer interfacesv4;
    Ipv6InterfaceContainer interfacesv6;

    if (addressMode == "Ipv4") {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        interfacesv4 = address.Assign(devices);
    } else if (addressMode == "Ipv6") {
        Ipv6AddressHelper address;
        address.SetBase("2001:db8::", Ipv6Prefix(64));
        interfacesv6 = address.Assign(devices);
    } else {
        NS_FATAL_ERROR("Invalid addressMode: " << addressMode);
    }

    uint16_t port = 4000;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 320;
    Time interval = MilliSeconds(50);

    UdpEchoClientHelper client;
    if (addressMode == "Ipv4") {
        client = UdpEchoClientHelper(interfacesv4.GetAddress(1), port);
    } else {
        client = UdpEchoClientHelper(interfacesv6.GetAddress(1, 1), port);
    }
    client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    client.SetAttribute("Interval", TimeValue(interval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}