#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoCsmaSimulation");

int main(int argc, char *argv[]) {
    bool useIpv6 = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 if true, else use IPv4", useIpv6);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(4);

    NS_LOG_INFO("Creating CSMA channel.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper internet;
    if (useIpv6) {
        internet.SetIpv4StackEnabled(false);
        internet.SetIpv6StackEnabled(true);
    } else {
        internet.SetIpv4StackEnabled(true);
        internet.SetIpv6StackEnabled(false);
    }
    internet.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper ipv4;
    Ipv6AddressHelper ipv6;
    Ipv4InterfaceContainer interfacesv4;
    Ipv6InterfaceContainer interfacesv6;

    if (useIpv6) {
        ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        interfacesv6 = ipv6.Assign(devices);
    } else {
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        interfacesv4 = ipv4.Assign(devices);
    }

    NS_LOG_INFO("Setting up applications.");
    uint16_t port = 9;

    // Server node n1
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    // Client node n0
    UdpEchoClientHelper client(useIpv6 ? interfacesv6.GetAddress(1, 1) : interfacesv4.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(9)); // From 2s to 10s at 1s interval = 9 packets
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(11.0));

    NS_LOG_INFO("Enabling tracing.");
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
    csma.EnablePcapAll("udp-echo", false);

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation completed.");
    return 0;
}