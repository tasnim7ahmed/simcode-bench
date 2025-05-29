#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create nodes: eNodeB, UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Set mobility for eNodeB (stationary)
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    // Set mobility for UE (random)
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(0.0),
                                    "MinY", DoubleValue(30.0),
                                    "DeltaX", DoubleValue(5.0),
                                    "DeltaY", DoubleValue(5.0),
                                    "GridWidth", UintegerValue(1),
                                    "LayoutType", StringValue("RowFirst"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)),
                                "Distance", DoubleValue(20.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UE and/or remote host behind EPC
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to the eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Setup default gateway for UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install applications: UDP server on UE, client on eNodeB through remote host (EPC)
    uint16_t serverPort = 4000;

    // Create a remote host to act as client, connect to PGW
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    // Connect remote host to PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    // Set default route on remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // UDP server on UE
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // UDP client on remoteHost
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(remoteHost);
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(10.0));

    // Enable tracing
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}