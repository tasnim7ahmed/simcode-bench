#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTraceClientServerSimulation");

int main(int argc, char *argv[]) {
    bool enableLogging = false;
    bool useIpv6 = false;
    std::string traceFile = "n0-0.tr";

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableLogging", "Enable detailed logging", enableLogging);
    cmd.AddValue("useIpv6", "Use IPv6 addressing instead of IPv4", useIpv6);
    cmd.AddValue("traceFile", "Trace file for client packet data", traceFile);
    cmd.Parse(argc, argv);

    if (enableLogging) {
        LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
        NS_LOG_INFO("Logging enabled.");
    }

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Create CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices = csma.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4InterfaceContainer ipv4Interfaces;
    Ipv6InterfaceContainer ipv6Interfaces;

    if (useIpv6) {
        // IPv6 setup
        Ipv6AddressHelper ipv6;
        ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
        ipv6Interfaces = ipv6.Assign(devices);
        ipv6Interfaces.SetForwarding(1, true);  // Enable forwarding on server node
    } else {
        // IPv4 setup
        Ipv4AddressHelper ipv4;
        ipv4.SetBase("10.1.1.0", "255.255.255.0");
        ipv4Interfaces = ipv4.Assign(devices);
    }

    // Set up UDP Server on n1
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Client on n0
    UdpClientHelper udpClient;
    if (useIpv6) {
        udpClient.SetRemote(ipv6Interfaces.GetAddress(1, 1), port);
    } else {
        udpClient.SetRemote(ipv4Interfaces.GetAddress(1), port);
    }
    udpClient.SetAttribute("MaxPackets", UintegerValue(0));  // Unlimited packets
    udpClient.SetAttribute("PacketSize", UintegerValue(1472));  // MTU - IP/UDP headers
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));  // Interval between packets

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Read trace file for sending packets
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(traceFile);
    clientApp.Get(0)->TraceConnectWithoutContext("Tx", MakeBoundCallback(&AsciiTraceHelper::DefaultEnqueueSinkWithContext, stream));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}