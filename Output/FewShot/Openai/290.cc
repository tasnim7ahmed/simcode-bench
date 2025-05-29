#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable LTE tracing
    LogComponentEnable("LtePhy", LOG_LEVEL_INFO);
    LogComponentEnable("LteEnbPhy", LOG_LEVEL_INFO);
    LogComponentEnable("LteUePhy", LOG_LEVEL_INFO);
    LogComponentEnable("LteMac", LOG_LEVEL_INFO);
    LogComponentEnable("LteRlc", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 eNodeB, 1 UE
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install Mobility Model
    MobilityHelper mobility;

    // eNodeB: stationary at the center
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator("ns3::ListPositionAllocator",
        "PositionList", VectorValue(Vector(50.0, 50.0, 0.0)));
    mobility.Install(enbNodes);

    // UE: Random movement within 100x100
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UE
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Set up default EPS bearer
    enum EpsBearer::Qci q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
    EpsBearer bearer(q);
    lteHelper->ActivateDataRadioBearer(ueDevs, bearer);

    // Applications
    // UDP server on UE, port 5000
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on eNodeB, sending to the UE's IP address
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    // For applications, install an internet stack on eNodeB (needed for client)
    internet.Install(enbNodes);

    // Assign IP to eNodeB (not strictly necessary, but allows app to bind)
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("7.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer enbIpIfaces = ipv4.Assign(enbDevs);

    ApplicationContainer clientApps = udpClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set routing between eNodeB and UE via LTE
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNodes.Get(0)->GetObject<Ipv4>()->GetRoutingProtocol());
    ueStaticRouting->SetDefaultRoute(Ipv4Address("7.0.0.1"), 1);

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}