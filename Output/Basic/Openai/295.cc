#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);

    // Install Mobility
    // eNodeB: stationary
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    // UE: random mobility
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(10.0));
    ueMobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                    "X", DoubleValue(20.0),
                                    "Y", DoubleValue(20.0),
                                    "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=30]"));
    ueMobility.Install(ueNodes);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UE ONLY (PGW will be gateway for eNB side, but we want applications at eNB node)
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    Ptr<EpcHelper> epcHelper = lteHelper->GetEpcHelper();
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Add a default route for the UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Applications: UDP Server on UE, UDP Client on eNodeB
    uint16_t serverPort = 4000;
    UdpServerHelper server(serverPort);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);

    UdpClientHelper client(ueIpIface.GetAddress(0), serverPort);
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}