#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[]) {
    bool useIpv6 = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 if true, IPv4 otherwise", useIpv6);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(4);

    NS_LOG_INFO("Creating CSMA channel.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    NS_LOG_INFO("Installing internet stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper addressV4;
    addressV4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv6AddressHelper addressV6;
    addressV6.SetBase("2001:db01::", Ipv6Prefix(64));

    Ipv4InterfaceContainer interfacesV4;
    Ipv6InterfaceContainer interfacesV6;

    if (useIpv6) {
        interfacesV6 = addressV6.Assign(devices);
        addressV6.NewNetwork();
    } else {
        interfacesV4 = addressV4.Assign(devices);
        addressV4.NewNetwork();
    }

    NS_LOG_INFO("Installing UDP echo server and client applications.");
    uint16_t port = 9;

    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    UdpEchoClientHelper client(useIpv6 ? interfacesV6.GetAddress(1, 1) : interfacesV4.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(9));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Setting up tracing.");
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    csma.EnableAsciiAll(stream);
    csma.EnablePcapAll("udp-echo");

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Done.");
    return 0;
}