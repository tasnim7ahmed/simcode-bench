#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/global-route-manager.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[]) {
    bool useIpv6 = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 if true, else use IPv4", useIpv6);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(4);

    NodeContainer n0n1n2n3 = nodes;

    NS_LOG_INFO("Creating CSMA channel.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(n0n1n3n2);

    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper internet;
    internet.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    if (useIpv6) {
        Ipv6AddressHelper address;
        address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        Ipv6InterfaceContainer interfaces = address.Assign(devices);
    } else {
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
    }

    NS_LOG_INFO("Setting up UDP Echo server on node 1.");
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    NS_LOG_INFO("Setting up UDP Echo client on node 0.");
    UdpEchoClientHelper echoClient(useIpv6 ? interfaces.GetAddress(1, 1) : interfaces.GetAddress(1, 0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(9));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
    csma.EnablePcapAll("udp-echo", false);

    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation completed.");

    return 0;
}