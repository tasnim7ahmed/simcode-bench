#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time simTime = Seconds(10.0);

    // Create nodes: 1 eNodeB, 1 UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility Model
    MobilityHelper mobility;

    // eNodeB: fixed position
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(enbPositionAlloc);
    mobility.Install(enbNodes);

    // UE: random walk in 100x100 area
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
        "Distance", DoubleValue(10.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
        "Time", TimeValue(Seconds(1.0)));
    mobility.Install(ueNodes);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetAttribute("UseIdealRrc", BooleanValue(true));
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper(epcHelper);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes); // Required for application traffic

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set default gateway for UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // UDP server (on UE)
    uint16_t port = 5000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(simTime);

    // UDP client (on eNodeB), sends to UE
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(simTime);

    // Enable LTE PHY, MAC, and RLC tracing
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    Simulator::Stop(simTime);
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}