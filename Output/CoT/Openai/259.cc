#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/nr-module.h"
#include "ns3/antenna-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    UintegerValue udpPacketSize = UintegerValue(1024);
    UintegerValue udpNumPackets = UintegerValue(5);

    CommandLine cmd(__FILE__);
    cmd.AddValue("udpPacketSize", "Size of each UDP packet", udpPacketSize);
    cmd.AddValue("udpNumPackets", "Number of UDP packets", udpNumPackets);
    cmd.Parse(argc, argv);

    Time simTime = Seconds(10.0);

    // Create nodes: 1 gNB, 2 UEs
    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // NR helper configuration
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerTdmaRR"));
    Ptr<ThreeGppChannelModel> cm = CreateObject<ThreeGppChannelModel>();
    nrHelper->SetChannelModel(cm);

    // Core network: EPC (5GC) and internet
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Internet stack
    InternetStackHelper internet;
    internet.Install(gNbNodes);
    internet.Install(ueNodes);
    internet.Install(pgw);

    // NR device config
    double frequency = 28e9; // 28 GHz mmWave
    double bandwidth = 100e6; // 100 MHz bandwidth

    // Assign spectrum (carrier) for this band
    NrSpectrumChannelHelper spectrumChannelHelper = NrSpectrumChannelHelper::Default();
    Ptr<NrSpectrumChannel> spectrumChannel = spectrumChannelHelper.Create();

    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    const uint8_t numBands = 1;
    std::vector<NrEnbBwp> allBwps = ccBwpCreator.CreateCcBwpVector(numCcPerBand, bandwidth, frequency, numBands);

    NrGnbPhyHelper gnbPhyHelper;
    gnbPhyHelper.SetChannel(spectrumChannel);
    NrUePhyHelper uePhyHelper;
    uePhyHelper.SetChannel(spectrumChannel);

    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes, allBwps);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, allBwps);

    // Mobility
    MobilityHelper mobility;
    // gNB is stationary at (0,0,10)
    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();
    gnbPositionAlloc->Add(Vector(0.0, 0.0, 10.0));
    mobility.SetPositionAllocator(gnbPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    // UEs: random walk in a 100x100 area at 1.5 m height
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue("Time"),
                             "Time", TimeValue(Seconds(1.0)),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=3.0]"),
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(ueNodes);

    // Attach UEs to gNB
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        nrHelper->Attach(ueDevs.Get(i), gNbDevs.Get(0));
    }

    // Activate default EPS bearers
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        nrHelper->ActivateDefaultEpsBearer(ueDevs.Get(i), EpsBearer(EpsBearer::NGBR_LOW_LAT_EMBB));
    }

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpInterfaces;
    ueIpInterfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set routing for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP Echo server on 2nd UE (index 1) on port 9
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(simTime);

    // UDP Echo client on 1st UE (index 0), send 5 packets of 1024 bytes to the 2nd UE
    UdpEchoClientHelper echoClient(ueIpInterfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", udpNumPackets);
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", udpPacketSize);

    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(simTime);

    // Enable logging and tracing as needed
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Simulator::Stop(simTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}