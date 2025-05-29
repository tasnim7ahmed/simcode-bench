#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create nodes
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);

    // Install Mobility
    // eNodeB static
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    // UE: random walk in 50x50 area
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                                "Distance", DoubleValue(10.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10.0]"));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    NodeContainer ueRsgw;
    ueRsgw.Add(ueNodes);
    ueRsgw.Add(epcHelper->GetPgwNode());
    internet.Install(ueRsgw);

    // Assign IP to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Static default route from UE to PGW
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Applications setup
    // UDP server on eNodeB (simulating HTTP server)
    uint16_t serverPort = 80;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client on UE (HTTP GET simulation: 5 packets, 1s intervals)
    UdpClientHelper udpClient;
    // Get eNodeB IP for UE to send to
    Ptr<Ipv4> ipv4 = enbNodes.Get(0)->GetObject<Ipv4>();
    Ipv4Address enbIpAddr = ipv4->GetAddress(1, 0).GetLocal();

    udpClient.SetAttribute("RemoteAddress", Ipv4AddressValue(enbIpAddr));
    udpClient.SetAttribute("RemotePort", UintegerValue(serverPort));
    udpClient.SetAttribute("MaxPackets", UintegerValue(5));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}