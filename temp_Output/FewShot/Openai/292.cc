#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/eps-bearer-tag.h"
#include "ns3/config-store-module.h"
#include "ns3/nr-module.h"
#include "ns3/epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: 1 gNB, 1 UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    // Install mobility: gNB stationary, UE moves
    MobilityHelper mobilityGnb;
    Ptr<ListPositionAllocator> gnbPosAlloc = CreateObject<ListPositionAllocator>();
    gnbPosAlloc->Add(Vector(0.0, 0.0, 10.0));
    mobilityGnb.SetPositionAllocator(gnbPosAlloc);
    mobilityGnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGnb.Install(gnbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobilityUe.Install(ueNodes);
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetPosition(Vector(10.0, 0.0, 1.5));
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(1.0, 0.0, 0.0));

    // Core/EPC/NR helpers
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetAttribute("Numerology", UintegerValue(1));
    nrHelper->SetAttribute("Frequency", DoubleValue(28e9)); // 28 GHz

    // Spectrum: 100 MHz band
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(1));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(1));
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(2));

    BandwidthPartInfo bwPart;
    bwPart.m_frequency = 28e9;
    bwPart.m_bandwidth = 100e6;
    bwPart.m_numRbs = 66;
    std::vector<BandwidthPartInfo> bwpVector;
    bwpVector.push_back(bwPart);
    nrHelper->SetBwpManagerAlgorithmAttribute("BwpBandWidthParts", BwpVectorValue(bwpVector));

    // Devices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UEs to gNB
    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // IP stack for UE, remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Create remote host (outward from EPC)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);

    // Set up point-to-point link between EPC and remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHostContainer.Get(0));

    // Assign IP address to remote host point-to-point link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set up default route on remote host
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (remoteHostContainer.Get(0)->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo("7.0.0.0", "255.0.0.0", 1);

    // Assign IP address to UE via EPC
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set up default route for UE nodes
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ipv4);
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Install UDP server on remote host (acts as server)
    uint16_t udpPort = 50000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHostContainer.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on UE, sending to remote host
    UdpClientHelper udpClient(remoteHostAddr, udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}