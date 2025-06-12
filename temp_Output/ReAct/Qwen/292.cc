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
    uint16_t numGnbs = 1;
    uint16_t numUesPerGnb = 1;
    double simTime = 10.0;
    double interPacketInterval = 0.5; // seconds

    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(512));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(Seconds(interPacketInterval)));
    Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MilliSeconds(100)));
    Config::SetDefault("ns3::LteRlcUm::ReportBufferStatusTimer", TimeValue(MilliSeconds(100)));

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<EpcHelper> epcHelper = EpcHelper::GetEpcHelper("ns3::PointToPointEpcHelper");
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetSchedulerType("ns3::nr::bwp::BwpRRScheduler");

    NodeContainer gNbNodes;
    gNbNodes.Create(numGnbs);

    NodeContainer ueNodes;
    ueNodes.Create(numUesPerGnb);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Distance", DoubleValue(20.0),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=0.5|Max=1.5]"));
    mobility.Install(ueNodes);

    NetDeviceContainer gnbNetDev;
    NetDeviceContainer ueNetDev;

    nrHelper->SetChannelConditionModelAttribute("always-los");
    nrHelper->SetPathlossModelType("ns3::ThreeGppUmiStreetCanyonPropagationLossModel");
    nrHelper->SetPathlossModelAttribute("Scenario", StringValue("UMi-Street-Canyon"));

    gnbNetDev = nrHelper->InstallGnbDevice(gNbNodes, BandwidthPartInfoPtr(new BandwidthPartInfo()));
    ueNetDev = nrHelper->InstallUeDevice(ueNodes, BandwidthPartInfoPtr(new BandwidthPartInfo()));

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gNbNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(ueNetDev);

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<NetDevice> ueDevice = ueNetDev.Get(u);
        Ipv4Address ueAddr = ueIpIface.GetAddress(u, 0);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        NS_ASSERT_MSG(ueStaticRouting, "No static routing for UE node");
        ueStaticRouting->SetDefaultRoute(epcHelper->GetPgwNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 1);
    }

    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ipv4Address remoteAddr = epcHelper->GetPgwNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
        UdpServerHelper server(8000);
        serverApps.Add(server.Install(gNbNodes.Get(0)));
        serverApps.Start(Seconds(0.01));

        UdpClientHelper client(remoteAddr, 8000);
        client.SetAttribute("MaxPackets", UintegerValue((simTime - 0.01) / interPacketInterval));
        clientApps.Add(client.Install(ueNodes.Get(u)));
        clientApps.Start(Seconds(0.01));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}