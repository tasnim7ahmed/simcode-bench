#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    uint32_t numUes = 1;
    uint32_t numGnb = 1;
    double simTime = 10.0; // seconds
    double udpInterval = 0.5; // 500 ms
    uint32_t udpPacketSize = 512;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetAttribute("Beamforming", BooleanValue(false));
    nrHelper->Initialize();

    NodeContainer gNbNodes;
    NodeContainer ueNodes;
    gNbNodes.Create(numGnb);
    ueNodes.Create(numUes);

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));

    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);
    Ipv4AddressHelper ipv4hlp;
    ipv4hlp.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4hlp.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    internet.Install(ueNodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(ueNodes);

    Ptr<ConstantVelocityMobilityModel> ueMobility = ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    ueMobility->SetPosition(Vector(10.0, 0.0, 1.5));
    ueMobility->SetVelocity(Vector(1.0, 0.0, 0.0)); // 1 m/s along X

    Ptr<ConstantPositionMobilityModel> gNbMobility = gNbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    gNbMobility->SetPosition(Vector(0.0, 0.0, 10.0));

    NetDeviceContainer gNbNetDev = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes);

    nrHelper->AttachToClosestEnb(ueNetDev, gNbNetDev);

    enum EpsBearer::Qci qci = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(qci);
    nrHelper->ActivateDedicatedEpsBearer(ueNetDev, bearer, EpcTft::Default());

    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
        Ptr<Node> ueNode = ueNodes.Get(j);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    uint16_t dlPort = 5000;
    UdpServerHelper udpServer(dlPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHostContainer.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper udpClient(remoteHostAddr, dlPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(uint32_t(simTime / udpInterval) + 10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(udpInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(udpPacketSize));

    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}