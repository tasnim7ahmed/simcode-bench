#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes: 1 eNodeB, 1 UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility Model: eNodeB fixed, UE random walk
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=-50|Max=50]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=-50|Max=50]"));
    ueMobility.Install(ueNodes);

    // LTE Devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.1.1.0", "255.255.255.0");
    NetDeviceContainer allDevs;
    allDevs.Add(ueLteDevs);
    allDevs.Add(enbLteDevs);
    Ipv4InterfaceContainer ipInterfaces = ipv4h.Assign(allDevs);

    // Set up LTE EPC
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Add remote host for client-side communication (simulate eNodeB as client)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    InternetStackHelper internetHost;
    internetHost.Install(remoteHost);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    NetDeviceContainer internetDevices = p2p.Install(pgw, remoteHost);

    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIpIf = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo("10.1.1.0", "255.255.255.0", 1);

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Install UDP Echo server on UE, port 9
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo client on eNodeB (or remoteHost to simulate traffic from outside)
    // We'll send from remoteHost as normally eNodeB does not generate traffic by itself
    // Find UE IP address
    Ipv4Address ueIpAddr = ipInterfaces.GetAddress(0);

    UdpEchoClientHelper echoClient(ueIpAddr, echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = echoClient.Install(remoteHost);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable tracing (optional)
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}