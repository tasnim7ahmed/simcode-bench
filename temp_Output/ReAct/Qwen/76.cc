#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedP2PCSMA");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer p2pNodes0;
    p2pNodes0.Create(2);
    NodeContainer p2pNodes1;
    p2pNodes1.Add(p2pNodes0.Get(1)); // n2
    Ptr<Node> n1 = NodeContainer::Create();
    p2pNodes1.Add(n1); // n1 to n2

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices0;
    p2pDevices0 = pointToPoint.Install(p2pNodes0);

    NetDeviceContainer p2pDevices1;
    p2pDevices1 = pointToPoint.Install(p2pNodes1);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes0.Get(1)); // n2
    csmaNodes.Create(4); // n3, n4, n5

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NodeContainer p2pNodes2;
    p2pNodes2.Add(csmaNodes.Get(3)); // n5
    p2pNodes2.Create(1); // n6

    NetDeviceContainer p2pDevices2;
    p2pDevices2 = pointToPoint.Install(p2pNodes2);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces0;
    p2pInterfaces0 = address.Assign(p2pDevices0);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1;
    p2pInterfaces1 = address.Assign(p2pDevices1);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces2;
    p2pInterfaces2 = address.Assign(p2pDevices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t packetSize = 50;
    double interval = (double)packetSize * 8 / 300; // seconds per packet

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(p2pInterfaces2.GetAddress(1), 9));
    onoff.SetConstantRate(DataRate("300bps"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer apps = onoff.Install(p2pNodes0.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(p2pNodes2.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-p2p-csma.tr");
    csma.EnableAsciiAll(stream);
    csma.EnablePcapAll("mixed-p2p-csma");
    pointToPoint.EnablePcapAll("mixed-p2p-csma");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}