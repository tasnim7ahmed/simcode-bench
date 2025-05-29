#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable basic UDP server/client logs
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: 1 eNodeB + 3 UEs
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(3);
    enbNodes.Create(1);

    // Install mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));      // eNodeB position
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));    // UE 1
    positionAlloc->Add(Vector(150.0, 20.0, 0.0));   // UE 2
    positionAlloc->Add(Vector(200.0, -20.0, 0.0));  // UE 3
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // LTE Helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install IP stack on UEs and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Configure a remote host (the EPC gateway is the server endpoint for UEs)
    Ptr<Node> remoteHost = CreateObject<Node>();
    internet.Install(remoteHost);

    // Set up remote host <-> EPC PGW connection
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer remoteHostDevices = p2ph.Install(remoteHost, epcHelper->GetPgwNode());

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIfaces = ipv4h.Assign(remoteHostDevices);

    // Set static routing so remoteHost can reach UEs via EPC PGW
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (remoteHost->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Assign UE IP addresses
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set default gateway for the UEs
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(
            ueNodes.Get(u)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP server (runs on remote host, port 9)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client apps on UEs (send to remote host IP, port 9)
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(remoteHostIfaces.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}