#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ripng-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RipNgSplitHorizon");

int main(int argc, char *argv[]) {
    bool enablePcap = true;
    bool enableAscii = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("pcap", "Enable pcap tracing", enablePcap);
    cmd.AddValue("ascii", "Enable ascii tracing", enableAscii);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(6);
    Ptr<Node> src = nodes.Get(0);
    Ptr<Node> a = nodes.Get(1);
    Ptr<Node> b = nodes.Get(2);
    Ptr<Node> c = nodes.Get(3);
    Ptr<Node> d = nodes.Get(4);
    Ptr<Node> dst = nodes.Get(5);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devSrcA;
    devSrcA = p2p.Install(src, a);

    NetDeviceContainer devAB;
    devAB = p2p.Install(a, b);

    NetDeviceContainer devAC;
    devAC = p2p.Install(a, c);

    NetDeviceContainer devBC;
    devBC = p2p.Install(b, c);

    NetDeviceContainer devBD;
    devBD = p2p.Install(b, d);

    NetDeviceContainer devDC;
    devDC = p2p.Install(c, d);
    // Cost for C-D link is 10
    DynamicCast<PointToPointChannel>(devDC.Get(0)->GetChannel())->SetAttribute("Delay", StringValue("20ms"));

    NetDeviceContainer devDstD;
    devDstD = p2p.Install(dst, d);

    InternetStackHelper internet;
    RipNgHelper ripNg;
    ripNg.SetInterfaceExclusions(a->GetObject<Ipv6>()->GetInterfaceForDevice(devAB.Get(0))->GetInterfaceIndex());
    ripNg.SetInterfaceExclusions(a->GetObject<Ipv6>()->GetInterfaceForDevice(devAC.Get(0))->GetInterfaceIndex());
    ripNg.SetInterfaceExclusions(d->GetObject<Ipv6>()->GetInterfaceForDevice(devDstD.Get(1))->GetInterfaceIndex());
    ripNg.SetSplitHorizonStrategy(RipNg::SPLIT_HORIZON);

    Ipv6ListRoutingHelper listRH;
    listRH.Add(ripNg, 0);

    internet.SetIpv6StackInstall(true);
    internet.SetRoutingHelper(listRH);
    internet.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iSrcA = ipv6.Assign(devSrcA);
    iSrcA.SetForwarding(0, true);
    iSrcA.SetDefaultRouteInAllNodes(0);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iAB = ipv6.Assign(devAB);
    iAB.SetForwarding(0, true);
    iAB.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iAC = ipv6.Assign(devAC);
    iAC.SetForwarding(0, true);
    iAC.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iBC = ipv6.Assign(devBC);
    iBC.SetForwarding(0, true);
    iBC.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iBD = ipv6.Assign(devBD);
    iBD.SetForwarding(0, true);
    iBD.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iDC = ipv6.Assign(devDC);
    iDC.SetForwarding(0, true);
    iDC.SetForwarding(1, true);

    ipv6.NewNetwork();
    Ipv6InterfaceContainer iDstD = ipv6.Assign(devDstD);
    iDstD.SetForwarding(0, true);
    iDstD.SetDefaultRouteInAllNodes(0);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(dst);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient(iDstD.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(src);
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(60.0));

    Simulator::Schedule(Seconds(40.0), &PointToPointChannel::DisableRunning, devBD.Get(0)->GetChannel());
    Simulator::Schedule(Seconds(44.0), &PointToPointChannel::EnableRunning, devBD.Get(0)->GetChannel());

    AsciiTraceHelper ascii;
    if (enableAscii) {
        Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("ripng-split-horizon.tr");
        internet.EnableAsciiIpv6Internal(stream, nodes);
    }

    if (enablePcap) {
        p2p.EnablePcapAll("ripng-split-horizon", false);
    }

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}