/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 10.0;

    // Enable logging for debugging as needed.
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Create eNodeB and UEs
    NodeContainer ueNodes;
    ueNodes.Create(3);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install mobility - eNodeB static, UEs random walk
    MobilityHelper enbMobility, ueMobility;

    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    ueMobility.SetMobilityModel(
        "ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)),
        "Distance", DoubleValue(2.0), // step size is not critical
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the Internet Stack (EPC)
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    InternetStackHelper internet;
    // Install on UEs and remote host
    internet.Install(ueNodes);
    internet.Install(pgw);

    // Assign IP to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Configure and deploy UDP server (at eNodeB side, via a virtual remote host)
    // We use the EPC's remote host as the server, to which the UEs can send.
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Connect remote host to PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Add internet stack to remote host
    internet.Install(remoteHost);

    // Assign IP address to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIf = ipv4h.Assign(internetDevices);

    // Setup routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install UDP server on remote host at port 9
    uint16_t serverPort = 9;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP clients on UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        // UEs send packets to the remote host
        UdpClientHelper udpClient(remoteHostIf.GetAddress(0), serverPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simTime));
    }

    // Enable basic tracing
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}