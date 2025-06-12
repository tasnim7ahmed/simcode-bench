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

    NodeContainer p2pNodes01;
    p2pNodes01.Create(2);

    NodeContainer p2pNodes56;
    p2pNodes56.Add(p2pNodes01.Get(1));
    p2pNodes56.Create(1);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes01.Get(1));
    csmaNodes.Create(3);

    NodeContainer csmaNode5 = csmaNodes.Get(3);
    p2pNodes56.Add(csmaNode5);
    p2pNodes56.Get(1)->CreateNode();

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("500Kbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices01 = pointToPoint.Install(p2pNodes01);
    NetDeviceContainer p2pDevices56 = pointToPoint.Install(p2pNodes56);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces01 = address.Assign(p2pDevices01);
    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);
    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces56 = address.Assign(p2pDevices56);

    uint32_t port = 9;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(p2pNodes56.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("448Kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(210));

    ApplicationContainer sourceApps;
    InetSocketAddress dst(p2pInterfaces56.GetAddress(1), port);
    onoff.SetAttribute("Remote", AddressValue(dst));
    sourceApps = onoff.Install(p2pNodes01.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("mixed-p2p-csma.tr");
    pointToPoint.EnableAsciiAll(stream);
    csma.EnableAsciiAll(stream);

    pointToPoint.EnablePcapAll("mixed-p2p-csma");
    csma.EnablePcapAll("mixed-p2p-csma");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}