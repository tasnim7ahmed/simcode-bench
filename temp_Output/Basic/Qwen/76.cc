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

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer p2pNodes0;
    p2pNodes0.Create(2);
    NodeContainer p2pNodes1;
    p2pNodes1.Add(p2pNodes0.Get(1)); // n2
    p2pNodes1.Create(1);             // n1 -> n2

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices0;
    p2pDevices0 = pointToPoint.Install(p2pNodes0);

    NetDeviceContainer p2pDevices1;
    p2pDevices1 = pointToPoint.Install(p2pNodes1);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes0.Get(1)); // n2
    csmaNodes.Create(4);             // n3, n4, n5

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NodeContainer p2pNodes2;
    p2pNodes2.Add(csmaNodes.Get(3)); // n5
    p2pNodes2.Create(1);             // n6

    NetDeviceContainer p2pDevices2;
    p2pDevices2 = pointToPoint.Install(p2pNodes2);

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

    uint16_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(p2pNodes2.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(50));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("300bps")));

    ApplicationContainer apps;
    AddressValue remoteAddress(InetSocketAddress(p2pInterfaces2.GetAddress(1), port));
    onoff.SetAttribute("Remote", remoteAddress);
    apps = onoff.Install(p2pNodes0.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-network.tr");
    pointToPoint.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    pointToPoint.EnablePcapAll("mixed-network");
    csma.EnablePcapAll("mixed-network");
    
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}