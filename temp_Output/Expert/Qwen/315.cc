#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGSimulation");

int main(int argc, char *argv[]) {
    uint32_t numUe = 1;
    uint32_t numGnb = 1;
    double simTime = 10.0;
    double gNbOffset = 0.0;
    uint32_t udpPacketSize = 1024;
    uint32_t numPackets = 1000;
    double packetInterval = 0.01;

    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(9999999));
    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(9999999));
    Config::SetDefault("ns3::NrMacSchedulerNs3::m_schedulerType",
                       EnumValue(NrMacSchedulerNs3::SchedulerType::PF));

    NodeContainer ueNodes;
    NodeContainer gNbNodes;
    ueNodes.Create(numUe);
    gNbNodes.Create(numGnb);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numUe; ++i) {
        positionAlloc->Add(Vector(0.0, i * 10.0, 0.0));
    }
    for (uint32_t i = 0; i < numGnb; ++i) {
        positionAlloc->Add(Vector(gNbOffset, i * 10.0, 0.0));
    }

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityUe.SetPositionAllocator(positionAlloc);
    mobilityUe.Install(ueNodes);

    MobilityHelper mobilityGnb;
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gNbNodes);

    NrPointToPointEpcHelper epcHelper;
    nr::NrHelper nrHelper;

    nrHelper.SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    nrHelper.SetPathlossModelType(TypeId::LookupByName("ns3::ThreeGppUmiStreetCanyonPropagationLossModel"));
    nrHelper.SetPathlossModelAttribute("ShadowingEnabled", BooleanValue(false));
    nrHelper.SetSpectrumPropagationLossModelType(
        TypeId::LookupByName("ns3::ThreeGppSpectrumPropagationLossModel"));
    nrHelper.SetSpectrumPropagationLossModelAttribute("ShadowingEnabled", BooleanValue(false));

    nrHelper.Initialize();
    Ptr<nr::BandwidthPartGnb> bwpGnb = nrHelper.CreateBandwidthPartGnb();
    bwpGnb->GetPhy()->SetAttribute("Numerology", UintegerValue(1));
    bwpGnb->SetAttribute("BwpId", UintegerValue(0));
    bwpGnb->SetAttribute("Scenario", StringValue("UMa"));

    NetDeviceContainer enbDevs = nrHelper.InstallGnbDevice(gNbNodes, bwpGnb);
    NetDeviceContainer ueDevs = nrHelper.InstallUeDevice(ueNodes, bwpGnb);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gNbNodes);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ueIpIfaces = ipv4h.Assign(ueDevs);
    Ipv4InterfaceContainer enbIpIfaces;
    for (uint32_t i = 0; i < enbDevs.GetN(); ++i) {
        Ptr<NetDevice> enbDev = enbDevs.Get(i);
        Ptr<Node> enbNode = enbDev->GetNode();
        for (uint32_t j = 0; j < enbNode->GetNDevices(); ++j) {
            if (enbNode->GetDevice(j)->GetObject<PointToPointNetDevice>()) {
                enbIpIfaces.Add(ipv4h.Assign(enbNode->GetDevice(j)));
            }
        }
    }

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueIpIfaces.Get(i).first->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper.GetUeDefaultGatewayAddress(), 1);
    }

    uint16_t dlPort = 9;
    UdpServerHelper dlServer(dlPort);
    ApplicationContainer serverApps = dlServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper dlClient(ueIpIfaces.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    dlClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    dlClient.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    ApplicationContainer clientApps = dlClient.Install(gNbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}