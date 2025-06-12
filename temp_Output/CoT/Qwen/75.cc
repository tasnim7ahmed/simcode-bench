#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedP2PCsmaSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer p2pNodes0, p2pNodes1, csmaNodes, p2pNodes2;
    p2pNodes0.Add(Names::Find<Node>("n0"));
    p2pNodes0.Add(Names::Find<Node>("n2"));
    p2pNodes1.Add(Names::Find<Node>("n1"));
    p2pNodes1.Add(Names::Find<Node>("n2"));
    csmaNodes.Add(Names::Find<Node>("n2"));
    csmaNodes.Add(Names::Find<Node>("n3"));
    csmaNodes.Add(Names::Find<Node>("n4"));
    csmaNodes.Add(Names::Find<Node>("n5"));
    p2pNodes2.Add(Names::Find<Node>("n5"));
    p2pNodes2.Add(Names::Find<Node>("n6"));

    PointToPointHelper pointToPoint0;
    pointToPoint0.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint0.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices0 = pointToPoint0.Install(p2pNodes0);

    PointToPointHelper pointToPoint1;
    pointToPoint1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint1.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices1 = pointToPoint1.Install(p2pNodes1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(6560)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    PointToPointHelper pointToPoint2;
    pointToPoint2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint2.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices2 = pointToPoint2.Install(p2pNodes2);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces0 = address.Assign(p2pDevices0);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces1 = address.Assign(p2pDevices1);

    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces2 = address.Assign(p2pDevices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(p2pInterfaces2.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(p2pNodes2.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("448Kbps"), 210);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer apps = onoff.Install(p2pNodes0.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(9.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-p2p-csma.tr");
    pointToPoint0.EnableAsciiAll(stream);
    pointToPoint1.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);
    pointToPoint2.EnableAsciiAll(stream);

    pointToPoint0.EnablePcapAll("mixed-p2p-csma");
    pointToPoint1.EnablePcapAll("mixed-p2p-csma");
    csma.EnablePcapAll("mixed-p2p-csma");
    pointToPoint2.EnablePcapAll("mixed-p2p-csma");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}