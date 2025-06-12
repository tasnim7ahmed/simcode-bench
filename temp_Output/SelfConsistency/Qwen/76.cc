#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedP2PCsmaNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer p2pNodes0, p2pNodes1, csmaNodes, p2pNodes2;
    p2pNodes0.Create(2);  // n0 and n2
    p2pNodes1.Add(p2pNodes0.Get(1));  // n2
    p2pNodes1.Create(1);  // n1 -> n2
    csmaNodes.Add(p2pNodes1.Get(1));  // n2
    csmaNodes.Create(3);  // n3, n4, n5
    p2pNodes2.Add(csmaNodes.Get(3));  // n5
    p2pNodes2.Create(1);  // n6

    // Point-to-point link between n0 and n2
    PointToPointHelper p2p0;
    p2p0.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p0.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices0 = p2p0.Install(p2pNodes0);

    // Point-to-point link between n1 and n2
    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices1 = p2p1.Install(p2pNodes1);

    // CSMA/CD network for n2, n3, n4, n5
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // Point-to-point link between n5 and n6
    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer p2pDevices2 = p2p2.Install(p2pNodes2);

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces0 = address.Assign(p2pDevices0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP server on n6 (sink)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(p2pNodes2.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create CBR/UDP flow from n0 to n6
    UdpEchoClientHelper echoClient(p2pInterfaces2.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // ~300 bps with 50 bytes packet
    echoClient.SetAttribute("PacketSize", UintegerValue(50));

    ApplicationContainer clientApps = echoClient.Install(p2pNodes0.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Setup tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-p2p-csma.tr");
    p2p0.EnableAsciiAll(stream);
    p2p1.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);
    p2p2.EnableAsciiAll(stream);

    // Enable pcap tracing
    p2p0.EnablePcapAll("mixed-p2p-csma");
    p2p1.EnablePcapAll("mixed-p2p-csma");
    csma.EnablePcapAll("mixed-p2p-csma");
    p2p2.EnablePcapAll("mixed-p2p-csma");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}