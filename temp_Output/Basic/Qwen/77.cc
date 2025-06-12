#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipngRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer srcNode;
    srcNode.Create(1);

    NodeContainer aNode;
    aNode.Create(1);

    NodeContainer bNode;
    bNode.Create(1);

    NodeContainer cNode;
    cNode.Create(1);

    NodeContainer dNode;
    dNode.Create(1);

    NodeContainer dstNode;
    dstNode.Create(1);

    PointToPointHelper p2pLowCost;
    p2pLowCost.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pLowCost.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pHighCost;
    p2pHighCost.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pHighCost.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer srcToA = p2pLowCost.Install(srcNode.Get(0), aNode.Get(0));
    NetDeviceContainer aToB = p2pLowCost.Install(aNode.Get(0), bNode.Get(0));
    NetDeviceContainer aToC = p2pLowCost.Install(aNode.Get(0), cNode.Get(0));
    NetDeviceContainer bToC = p2pLowCost.Install(bNode.Get(0), cNode.Get(0));
    NetDeviceContainer bToD = p2pLowCost.Install(bNode.Get(0), dNode.Get(0));
    NetDeviceContainer cToD = p2pHighCost.Install(cNode.Get(0), dNode.Get(0));
    NetDeviceContainer dToDst = p2pLowCost.Install(dNode.Get(0), dstNode.Get(0));

    InternetStackHelper internetv6;
    RipNgHelper ripNgRouting;

    ripNgRouting.SetInterfaceMetric(aToB.Get(0), 1);
    ripNgRouting.SetInterfaceMetric(aToC.Get(0), 1);
    ripNgRouting.SetInterfaceMetric(bToC.Get(0), 1);
    ripNgRouting.SetInterfaceMetric(bToD.Get(0), 1);
    ripNgRouting.SetInterfaceMetric(cToD.Get(0), 10);

    ripNgRouting.EnableSplitHorizon();
    internetv6.SetRoutingProtocol(ripNgRouting);
    internetv6.Install(NodeContainer::GetGlobal());

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));

    Ipv6InterfaceContainer srcToAIfs = ipv6.Assign(srcToA);
    Ipv6InterfaceContainer aToBIfs = ipv6.Assign(aToB);
    Ipv6InterfaceContainer aToCIfs = ipv6.Assign(aToC);
    Ipv6InterfaceContainer bToCIfs = ipv6.Assign(bToC);
    Ipv6InterfaceContainer bToDIfs = ipv6.Assign(bToD);
    Ipv6InterfaceContainer cToDIfs = ipv6.Assign(cToD);
    Ipv6InterfaceContainer dToDstIfs = ipv6.Assign(dToDst);

    srcToAIfs.SetForwarding(0, true);
    srcToAIfs.SetDefaultRouteInAllNodes(0);

    dToDstIfs.SetForwarding(1, true);
    dToDstIfs.SetDefaultRouteInAllNodes(1);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(dstNode.Get(0));
    serverApps.Start(Seconds(3.0));
    serverApps.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient(dToDstIfs.GetAddress(1, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(srcNode.Get(0));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(60.0));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("ripng-routing.tr");
    internetv6.EnableAsciiIpv6InternalRoutingAll(stream);
    internetv6.EnableAsciiIpv6AllRoutingTables(stream);

    p2pLowCost.EnablePcapAll("ripng-routing");

    Simulator::Schedule(Seconds(40.0), &PointToPointNetDevice::SetLinkEnabled, bToD.Get(1), false);
    Simulator::Schedule(Seconds(44.0), &PointToPointNetDevice::SetLinkEnabled, bToD.Get(1), true);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}