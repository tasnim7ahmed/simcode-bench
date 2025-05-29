#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint16_t numberOfUes = 3;
    uint16_t numberOfEnbs = 1;
    double simTime = 10.0;
    double distance = 60.0;

    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);
    NodeContainer enbNodes;
    enbNodes.Create(numberOfEnbs);

    // Install Mobility Model
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // eNodeB
    for (uint32_t i = 0; i < numberOfUes; ++i)
    {
        positionAlloc->Add(Vector(distance * (i + 1), 0.0, 0.0)); // UEs
    }
    mobility.SetPositionAllocator(positionAlloc);

    // eNodeB
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    // UEs
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);

    // LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP Addresses
    Ipv4InterfaceContainer ueIpIface;
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // PGW node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create Remote Host (optional for uplink/downlink server)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);

    // Connect PGW and RemoteHost
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Assign IP to UEs
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to eNB
    for (uint16_t i = 0; i < numberOfUes; i++)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Setup default routes
    for (uint32_t j = 0; j < ueNodes.GetN(); ++j)
    {
        Ptr<Node> ueNode = ueNodes.Get(j);
        Ptr<Ipv4> ipv4 = ueNode->GetObject<Ipv4>();
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ipv4);
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }
    // Set route for remoteHost
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper().GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Applications: UDP Server on eNodeB
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Retrieve eNodeB IP address (on its interface in EPC network)
    Ptr<Node> enbNode = enbNodes.Get(0);
    Ptr<Ipv4> enbIpv4 = enbNode->GetObject<Ipv4>();
    Ipv4Address enbIpAddr = enbIpv4->GetAddress(1, 0).GetLocal();

    // Applications: UDP Client on each UE targeting eNodeB
    for (uint32_t i = 0; i < numberOfUes; ++i)
    {
        UdpClientHelper udpClient(enbIpAddr, port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        udpClient.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simTime));
    }

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}