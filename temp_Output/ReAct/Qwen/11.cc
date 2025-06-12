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
    bool useIpv6 = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable detailed logging", enableLogging);
    cmd.AddValue("ipv6", "Use IPv6 instead of IPv4", useIpv6);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpCsmaSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_ALL);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_ALL);
    }

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Creating CSMA channel.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper internet;
    internet.Install(nodes);

    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6) {
        Ipv6AddressHelper ipv6;
        ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
    } else {
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
    }

    NS_LOG_INFO("Setting up UDP server on node 1.");
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Setting up UDP client on node 0.");
    UdpClientHelper client;
    if (useIpv6) {
        client = UdpClientHelper(ipv6Interfaces.GetAddress(1, 1), port);
    } else {
        client = UdpClientHelper(ipv4Interfaces.GetAddress(1), port);
    }
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Starting simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    return 0;
}