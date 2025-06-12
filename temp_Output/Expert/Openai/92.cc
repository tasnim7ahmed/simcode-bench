#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    NodeContainer src, rtr1, rtr2, drtr, dst;
    src.Create(1);
    rtr1.Create(1);
    rtr2.Create(1);
    drtr.Create(1);
    dst.Create(1);

    NodeContainer nSrcRtr1(src.Get(0), rtr1.Get(0));
    NodeContainer nSrcRtr2(src.Get(0), rtr2.Get(0));
    NodeContainer nRtr1DRtr(rtr1.Get(0), drtr.Get(0));
    NodeContainer nRtr2DRtr(rtr2.Get(0), drtr.Get(0));
    NodeContainer nDRtrDst(drtr.Get(0), dst.Get(0));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer dSrcRtr1 = p2p.Install(nSrcRtr1);
    NetDeviceContainer dSrcRtr2 = p2p.Install(nSrcRtr2);
    NetDeviceContainer dRtr1DRtr = p2p.Install(nRtr1DRtr);
    NetDeviceContainer dRtr2DRtr = p2p.Install(nRtr2DRtr);
    NetDeviceContainer dDRtrDst = p2p.Install(nDRtrDst);

    InternetStackHelper internet;
    internet.Install(src);
    internet.Install(rtr1);
    internet.Install(rtr2);
    internet.Install(drtr);
    internet.Install(dst);

    Ipv4AddressHelper ipv4;
    Ipv4InterfaceContainer iSrcRtr1, iSrcRtr2, iRtr1DRtr, iRtr2DRtr, iDRtrDst;

    ipv4.SetBase("10.0.1.0", "255.255.255.0");
    iSrcRtr1 = ipv4.Assign(dSrcRtr1);

    ipv4.SetBase("10.0.2.0", "255.255.255.0");
    iSrcRtr2 = ipv4.Assign(dSrcRtr2);

    ipv4.SetBase("10.0.3.0", "255.255.255.0");
    iRtr1DRtr = ipv4.Assign(dRtr1DRtr);

    ipv4.SetBase("10.0.4.0", "255.255.255.0");
    iRtr2DRtr = ipv4.Assign(dRtr2DRtr);

    ipv4.SetBase("10.0.5.0", "255.255.255.0");
    iDRtrDst = ipv4.Assign(dDRtrDst);

    Ipv4StaticRoutingHelper staticRoutingHelper;

    Ptr<Ipv4> ipv4Src = src.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4Rtr1 = rtr1.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4Rtr2 = rtr2.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4DRtr = drtr.Get(0)->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4Dst = dst.Get(0)->GetObject<Ipv4>();

    Ptr<Ipv4StaticRouting> sRoutingSrc = staticRoutingHelper.GetStaticRouting(ipv4Src);
    Ptr<Ipv4StaticRouting> sRoutingRtr1 = staticRoutingHelper.GetStaticRouting(ipv4Rtr1);
    Ptr<Ipv4StaticRouting> sRoutingRtr2 = staticRoutingHelper.GetStaticRouting(ipv4Rtr2);
    Ptr<Ipv4StaticRouting> sRoutingDRtr = staticRoutingHelper.GetStaticRouting(ipv4DRtr);
    Ptr<Ipv4StaticRouting> sRoutingDst = staticRoutingHelper.GetStaticRouting(ipv4Dst);

    sRoutingSrc->AddNetworkRouteTo("10.0.5.0", "255.255.255.0", iSrcRtr1.GetAddress(1), 1);
    sRoutingSrc->AddNetworkRouteTo("10.0.5.0", "255.255.255.0", iSrcRtr2.GetAddress(1), 2);

    sRoutingSrc->SetDefaultRoute(iSrcRtr1.GetAddress(1), 1);

    sRoutingRtr1->AddNetworkRouteTo("10.0.5.0", "255.255.255.0", iRtr1DRtr.GetAddress(1), 2);
    sRoutingRtr1->AddNetworkRouteTo("10.0.2.0", "255.255.255.0", iRtr1DRtr.GetAddress(0), 1);
    sRoutingRtr1->SetDefaultRoute(iRtr1DRtr.GetAddress(1), 2);

    sRoutingRtr2->AddNetworkRouteTo("10.0.5.0", "255.255.255.0", iRtr2DRtr.GetAddress(1), 2);
    sRoutingRtr2->AddNetworkRouteTo("10.0.1.0", "255.255.255.0", iRtr2DRtr.GetAddress(0), 1);
    sRoutingRtr2->SetDefaultRoute(iRtr2DRtr.GetAddress(1), 2);

    sRoutingDRtr->AddNetworkRouteTo("10.0.1.0", "255.255.255.0", iDRtrDst.GetAddress(0), 3);
    sRoutingDRtr->AddNetworkRouteTo("10.0.2.0", "255.255.255.0", iDRtrDst.GetAddress(0), 3);
    sRoutingDRtr->SetDefaultRoute(iDRtrDst.GetAddress(1), 3);

    sRoutingDst->SetDefaultRoute(iDRtrDst.GetAddress(0), 1);

    uint16_t port = 50000;

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(dst.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    BulkSendHelper bulkSend1("ns3::TcpSocketFactory", InetSocketAddress(iDRtrDst.GetAddress(1), port));
    bulkSend1.SetAttribute("MaxBytes", UintegerValue(0));
    bulkSend1.SetAttribute("SendSize", UintegerValue(1024));
    ApplicationContainer sourceApps1 = bulkSend1.Install(src.Get(0));
    sourceApps1.Start(Seconds(1.0));
    sourceApps1.Stop(Seconds(6.0));

    Simulator::Schedule(Seconds(6.1), [&](){
        src.Get(0)->GetObject<Ipv4>()->SetMetric(1, 16); // make path via Rtr1 less preferable
        src.Get(0)->GetObject<Ipv4>()->SetMetric(2, 1);  // path via Rtr2 most preferable
        BulkSendHelper bulkSend2("ns3::TcpSocketFactory", InetSocketAddress(iDRtrDst.GetAddress(1), port));
        bulkSend2.SetAttribute("MaxBytes", UintegerValue(0));
        bulkSend2.SetAttribute("SendSize", UintegerValue(1024));
        ApplicationContainer sourceApps2 = bulkSend2.Install(src.Get(0));
        sourceApps2.Start(Seconds(6.2));
        sourceApps2.Stop(Seconds(12.0));
    });

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}