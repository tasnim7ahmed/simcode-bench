#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedP2PCSMAExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer p2pNodes0;
    p2pNodes0.Create(2);
    NodeContainer p2pNodes1 = NodeContainer(p2pNodes0.Get(1), NodeContainer::Create(1).Get(0)); // n2 connects to n1

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes0.Get(1)); // n2
    csmaNodes.Create(4); // n3, n4, n5, n6 (n5 is last in this group)
    
    NodeContainer p2pNodes2 = NodeContainer(csmaNodes.Get(3), NodeContainer::Create(1).Get(0)); // n5 -> n6

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices0;
    p2pDevices0 = pointToPoint.Install(p2pNodes0);

    NetDeviceContainer p2pDevices1;
    p2pDevices1 = pointToPoint.Install(p2pNodes1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(0.0032)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NetDeviceContainer p2pDevices2;
    p2pDevices2 = pointToPoint.Install(p2pNodes2);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer p2pInterfaces0;
    Ipv4InterfaceContainer p2pInterfaces1;
    Ipv4InterfaceContainer csmaInterfaces;
    Ipv4InterfaceContainer p2pInterfaces2;

    address.SetBase("10.1.1.0", "255.255.255.0");
    p2pInterfaces0 = address.Assign(p2pDevices0);

    address.NewNetwork();
    p2pInterfaces1 = address.Assign(p2pDevices1);

    address.NewNetwork();
    csmaInterfaces = address.Assign(csmaDevices);

    address.NewNetwork();
    p2pInterfaces2 = address.Assign(p2pDevices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint32_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(p2pInterfaces2.GetAddress(1), port));
    onoff.SetConstantRate(DataRate("300bps"), 50);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer apps = onoff.Install(csmaNodes.Get(0)); // n2 as source
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(p2pNodes2.Get(1)); // n6 as sink
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiHelper;
    Ptr<OutputStreamWrapper> stream = asciiHelper.CreateFileStream("mixed-p2p-csma.tr");
    pointToPoint.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    pointToPoint.EnablePcapAll("mixed-p2p-csma");
    csma.EnablePcapAll("mixed-p2p-csma");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}