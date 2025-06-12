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
    std::string addressMode = "IPv4";

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable detailed logging", enableLogging);
    cmd.AddValue("addressMode", "Addressing mode: 'IPv4' or 'IPv6'", addressMode);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
        NS_LOG_INFO("Detailed logging enabled for UDP client and server.");
    }

    NS_LOG_INFO("Creating nodes n0 and n1.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Setting up CSMA channel with 5 Mbps data rate, 2 ms delay, MTU 1400 bytes.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    if (addressMode == "IPv4") {
        NS_LOG_INFO("Using IPv4 addressing.");
        stack.Install(nodes);
    } else if (addressMode == "IPv6") {
        NS_LOG_INFO("Using IPv6 addressing.");
        stack.SetIpv4StackInstall(false);
        stack.SetIpv6StackInstall(true);
        stack.Install(nodes);
    } else {
        NS_FATAL_ERROR("Invalid addressMode option. Use 'IPv4' or 'IPv6'.");
    }

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    if (addressMode == "IPv4") {
        interfaces = ipv4.Assign(devices);
    }

    Ipv6AddressHelper ipv6;
    if (addressMode == "IPv6") {
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        interfaces = ipv6.Assign(devices);
    }

    uint16_t port = 4000;

    NS_LOG_INFO("Installing UDP Server on node n1 at port " << port);
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Installing UDP Client on node n0.");
    UdpClientHelper client(interfaces.GetAddress(1, 0), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Enabling global routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Running simulation for 10 seconds.");
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}