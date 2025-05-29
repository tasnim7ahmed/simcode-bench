#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaUdpFlow");

int main(int argc, char *argv[])
{
    bool useIpv6 = false;
    bool enableLogging = false;

    CommandLine cmd;
    cmd.AddValue("useIpv6", "Enable IPv6 addressing", useIpv6);
    cmd.AddValue("enableLogging", "Enable application logging", enableLogging);
    cmd.Parse(argc, argv);

    if (enableLogging)
    {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable("CsmaUdpFlow", LOG_LEVEL_INFO);
    }

    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    NS_LOG_INFO("Configuring CSMA channel.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1400));

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    Ipv6AddressHelper ipv6;
    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    Address serverAddress;

    if (!useIpv6)
    {
        NS_LOG_INFO("Assigning IPv4 addresses.");
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
        serverAddress = Address(ipv4Interfaces.GetAddress(1));
    }
    else
    {
        NS_LOG_INFO("Assigning IPv6 addresses.");
        ipv6.SetBase("2001:0:0:1::", Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
        // Set all interfaces as not link-local
        ipv6Interfaces.SetForwarding(0, true);
        ipv6Interfaces.SetForwarding(1, true);
        ipv6Interfaces.SetDefaultRouteInAllNodes(0);

        serverAddress = Address(ipv6Interfaces.GetAddress(1,1));
    }

    uint16_t port = 4000;

    NS_LOG_INFO("Installing UDP Echo Server.");
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Installing UDP Echo Client.");
    UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(320));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(50)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Running simulation.");
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation finished.");
    return 0;
}