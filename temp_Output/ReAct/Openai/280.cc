#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Logging (optional, can be enabled for TCP)
    // LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);

    // Set simulation parameters
    double simTime = 10.0;

    // Create Nodes: 1 gNB, 2 UEs
    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install NR helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    // Set up core network (internet)
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Install gNB and UE devices
    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Mobility: gNB is fixed, UEs use RandomWalk2dMobilityModel in 50x50 area
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gNbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                               "Distance", DoubleValue(5.0));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMobility.Install(ueNodes);

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(gNbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to gNB
    nrHelper->AttachToClosestEnb(ueDevs, gNbDevs);

    // Set Default Gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Configure TCP application
    uint16_t serverPort = 8080;

    // Create TCP server on second UE
    Address serverAddress(InetSocketAddress(ueIfaces.GetAddress(1), serverPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer serverApp = packetSinkHelper.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Create TCP client on first UE
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    ApplicationContainer clientApp = clientHelper.Install(ueNodes.Get(0));

    // Enable pcap tracing (optional)
    // nrHelper->EnableDlPhyTraces();
    // nrHelper->EnableUlPhyTraces();

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}