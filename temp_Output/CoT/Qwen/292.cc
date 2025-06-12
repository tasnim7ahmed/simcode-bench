#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-module.h"
#include "ns3/epc-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Nr5GEpcSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<EpcHelper> epcHelper = EpcHelper::GetEpcHelper("ns3::EpcHelper");

    NodeContainer enbNodes;
    NodeContainer ueNodes;

    enbNodes.Create(1);
    ueNodes.Create(1);

    // Create and set mobility models
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobilityUe.Install(ueNodes);

    // Install NR devices
    NetDeviceContainer enbDevs;
    NetDeviceContainer ueDevs;

    nrHelper->SetGnbPhyAttribute("ChannelNumber", UintegerValue(1));
    nrHelper->SetUePhyAttribute("ChannelNumber", UintegerValue(1));

    nrHelper->SetGnbPhyAttribute("Numerology", UintegerValue(1));
    nrHelper->SetUePhyAttribute("Numerology", UintegerValue(1));

    nrHelper->SetGnbMacAttribute("UseAmc", BooleanValue(true));
    nrHelper->SetUeMacAttribute("UseAmc", BooleanValue(true));

    enbDevs = nrHelper->InstallGnbDevice(enbNodes, epcHelper->GetS1uGtpuSocketFactory());
    ueDevs = nrHelper->InstallUeDevice(ueNodes, epcHelper->GetS1uGtpuSocketFactory());

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to the gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        epcHelper->Attach(ueDevs.Get(i));
    }

    // Set default gateway for UEs
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Setup applications: UDP echo client on UE sending packets every 500ms
    uint32_t packetSize = 512;
    Time interPacketInterval = MilliSeconds(500);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(ueIpIfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // Infinite packets until simulation ends
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(10.0));

    // Enable logging
    LogComponentEnable("Nr5GEpcSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_ALL);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}