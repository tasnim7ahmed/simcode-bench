#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/on-off-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer p2pNodes;
    p2pNodes.Create(3);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(2));
    csmaNodes.Create(3);

    NodeContainer p2pEndNodes;
    p2pEndNodes.Create(1);

    NodeContainer allNodes;
    allNodes.Add(p2pNodes);
    allNodes.Add(csmaNodes);
    allNodes.Add(p2pEndNodes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pDevices02;
    p2pDevices02 = pointToPoint.Install(p2pNodes.Get(0), p2pNodes.Get(2));

    NetDeviceContainer p2pDevices12;
    p2pDevices12 = pointToPoint.Install(p2pNodes.Get(1), p2pNodes.Get(2));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    NetDeviceContainer p2pDevices56;
    p2pDevices56 = pointToPoint.Install(csmaNodes.Get(3), p2pEndNodes.Get(0));

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces02 = address.Assign(p2pDevices02);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces12 = address.Assign(p2pDevices12);

    address.NewNetwork();
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.NewNetwork();
    Ipv4InterfaceContainer p2pInterfaces56 = address.Assign(p2pDevices56);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(p2pInterfaces56.GetAddress(0), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(50));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("300bps")));

    ApplicationContainer apps = onoff.Install(p2pNodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(p2pEndNodes.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("mixed-network.tr"));
    csma.EnableAsciiAll(ascii.CreateFileStream("mixed-network-csma.tr"));

    pointToPoint.EnablePcapAll("mixed-network");
    csma.EnablePcapAll("mixed-network-csma");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}