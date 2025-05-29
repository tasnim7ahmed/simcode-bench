#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for UDP applications (optional)
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: Node 0 = eNB, Node 1 = UE
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install mobility: eNB static, UE random walk
    MobilityHelper mobilityEnb;
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    MobilityHelper mobilityUe;
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                                "Distance", DoubleValue(10.0),
                                "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobilityUe.Install(ueNodes);

    // Create LTE Helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Install LTE devices to eNB and UE
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UE and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(pgw);

    // Assign IP address to UEs, and use EPC to route
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Add static default route in UE towards gateway
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(0)->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Create a UDP server on eNB side (or PGW side) -- set as a "remote host"
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    internet.Install(remoteHostContainer);

    // Connect remoteHost to PGW using point-to-point
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Set static routing for remoteHost
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // UDP Server on remoteHost, Client on UE
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper udpClient(remoteHostAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}