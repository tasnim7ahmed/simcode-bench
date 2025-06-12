#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/nr-module.h"
#include "ns3/epc-helper.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint32_t numUes = 1;
    uint32_t numGnb = 1;
    double simTime = 10.0;
    double interPacketInterval = 0.5; // seconds

    NodeContainer ueNodes;
    NodeContainer gnbNodes;
    ueNodes.Create(numUes);
    gnbNodes.Create(numGnb);

    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetEpcHelper(epcHelper);

    // NR defaults: Use one band (Band n78, 3.5 GHz)
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    BandwidthPartInfoPtrVector allBwps = ccBwpCreator.CreateCcBwpSingleBand(
        nrHelper,
        NrHelper::BAND_ID_n78,
        numCcPerBand,
        BandwidthPartInfo::BWP_STANDARD,
        NrHelper::DEFAULT_TX_POWER,
        std::vector<uint16_t>());

    nrHelper->InitializeOperationBand(&allBwps);

    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    MobilityHelper mobility;
    // gNB stationary at (0, 0, 30)
    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();
    gnbPositionAlloc->Add(Vector(0.0, 0.0, 30.0));
    mobility.SetPositionAllocator(gnbPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);

    // UE moves in a straight line
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(ueNodes);
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetPosition(Vector(10.0, 0.0, 1.0));
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0.5, 0.0, 0.0)); // 0.5 m/s

    // Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IPs and configure EPC
    Ipv4InterfaceContainer ueIpIfaces;
    Ipv4Address remoteHostAddr;

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);

    // connect P2P between PGW and remoteHost
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    remoteHostAddr = internetIpIfaces.GetAddress(1);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(
        Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Attach UE and assign IP
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->Attach(ueDevs.Get(i), gnbDevs.Get(0));
    }
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    // set default route
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ue = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP server (sink) on remoteHost. UDP client on UE towards gNB's IP.
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper udpClient(remoteHostAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}