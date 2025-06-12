#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRlc", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4InterfaceContainer ueInterfaces;
    ueInterfaces = epcHelper->AssignUeIpv4Address(ueDevs);

    Ipv4Address pgwAddr = epcHelper->GetPgwNode()->GetObject<Ipv4>()->GetInterface(0)->GetAddress(0).GetLocal();
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Node> ue = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> routing = Ipv4RoutingHelper::GetRouting<UdpEchoClient>(ue->GetObject<Ipv4>())->
            GetStaticRouting();
        routing->SetDefaultRoute(pgwAddr, 1);
    }

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    internet.Install(remoteHostContainer);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    Ipv4InterfaceContainer remoteHostIfaces;
    remoteHostIfaces = epcHelper->InstallIpv4AddressForPgw(remoteHostContainer.Get(0));

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);

    Ipv4InterfaceContainer internetInterfaces;
    internetInterfaces.Add(epcHelper->AssignPgwIpv4Address(internetDevices.Get(0)));
    Ipv4Address remoteHostAddr = internetInterfaces.GetAddress(0);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    uint16_t dlPort = 1234;
    UdpEchoServerHelper echoServer(dlPort);
    ApplicationContainer serverApps = echoServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(remoteHostAddr, dlPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient.Install(ueNodes.Get(0));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = echoClient.Install(ueNodes.Get(1));
    clientApps2.Start(Seconds(3.0));
    clientApps2.Stop(Seconds(10.0));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    AnimationInterface anim("lte-udp-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.25));
    anim.UpdateNodeDescription(enbNodes.Get(0), "eNodeB");
    anim.UpdateNodeColor(enbNodes.Get(0), 255, 0, 0);
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        std::string desc = "UE" + std::to_string(i+1);
        anim.UpdateNodeDescription(ueNodes.Get(i), desc);
        anim.UpdateNodeColor(ueNodes.Get(i), 0, 0, 255);
    }
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}