#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 gNB, 1 UE, 1 remote host (represents CN)
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Install NR
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));
    nrHelper->SetAttribute("UseIdealRrc", BooleanValue(true));

    // 5G Core: Connect remote host to EPC
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.0001)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIfaces = ipv4h.Assign(internetDevices);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);
    mobility.Install(ueNodes);

    // NR Device configuration
    uint16_t gnbNumAntennas = 4;
    NrGnbPhyHelper gnbPhyHelper = NrGnbPhyHelper();
    NrUePhyHelper uePhyHelper = NrUePhyHelper();

    nrHelper->SetGnbPhyHelper(&gnbPhyHelper);
    nrHelper->SetUePhyHelper(&uePhyHelper);

    double gridStep = 10.0;
    NrGnbNetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, gnbNumAntennas);
    NrUeNetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Assign UE to cell
    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // Install IP stack on UE
    InternetStackHelper ueInternet;
    ueInternet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNodes.Get(j)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Application: UDP server on UE
    uint16_t serverPort = 9;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Application: UDP client on gNB
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(gnbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Internet on gNB node for client app (assign dummy IP, no real CN interface)
    InternetStackHelper gnbInt;
    gnbInt.Install(gnbNodes);

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}