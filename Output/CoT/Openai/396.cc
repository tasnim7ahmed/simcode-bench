#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: remote host (client), eNB, UE
    Ptr<Node> ueNode = CreateObject<Node>();
    Ptr<Node> enbNode = CreateObject<Node>();
    Ptr<Node> remoteHost = CreateObject<Node>();

    NodeContainer ueNodes;
    ueNodes.Add(ueNode);

    NodeContainer enbNodes;
    enbNodes.Add(enbNode);

    // Set up point-to-point link between remoteHost and EPC gateway
    NodeContainer remoteHostContainer;
    remoteHostContainer.Add(remoteHost);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));

    // Install Internet stack on UE/Remote host
    InternetStackHelper internet;
    internet.Install(remoteHost);

    // EPC & LTE
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Assign IP addresses to remote host/pgw
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Route from remoteHost to UE subnet through PGW
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Assign IP address to UE
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs.Get(0)));
    Ipv4Address ueIpAddr = ueIpIfaces.GetAddress(0);

    // Static route on UE to core
    Ptr<Node> ue = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // UDP server on UE (port 12345)
    uint16_t port = 12345;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(0.2));
    serverApp.Stop(Seconds(20.0));

    // UDP client on remoteHost (send to UE's IP)
    uint32_t maxPackets = 1000;
    Time interPacketInterval = MilliSeconds(20);
    UdpClientHelper client(ueIpAddr, port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(remoteHost);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(20.0));

    // Enable tracing (optional)
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}