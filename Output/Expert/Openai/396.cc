#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    uint16_t port = 12345;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    double interval = 0.02; // 20ms

    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(enbNodes);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("7.0.0.0", "255.0.0.0");

    Ipv4InterfaceContainer enbIpIface;
    Ipv4InterfaceContainer ueIpIface;

    // Attach UE to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Assign IP addresses to UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Node> enbNode = enbNodes.Get(0);

    // Create a P2P link between eNB and Remote Host for IP connectivity
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    InternetStackHelper internetRemote;
    internetRemote.Install(remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2ph.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer p2pDevs = p2ph.Install(enbNodes.Get(0), remoteHostContainer.Get(0));
    ipv4.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer p2pIfaces = ipv4.Assign(p2pDevs);

    // Assign UE IP address
    EpcHelper* epcHelper = lteHelper->GetEpcHelper();
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs.Get(0)));
    Ipv4Address ueAddr = ueIpIfaces.GetAddress(0);

    // Set default routes
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install UDP server on UE
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNode);
    serverApp.Start(Seconds(0.1));
    serverApp.Stop(Seconds(25));

    // Install UDP client on eNB (send to UE)
    UdpClientHelper udpClient(ueAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = udpClient.Install(enbNode);
    clientApps.Start(Seconds(0.2));
    clientApps.Stop(Seconds(25));

    // Enable IP on interfaces
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}