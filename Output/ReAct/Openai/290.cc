#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation time
    double simTime = 10.0;

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Position Allocators
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(50.0, 50.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                   "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                   "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue("Time"),
                                "Time", TimeValue(Seconds(0.5)),
                                "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                                "Bounds", RectangleValue(Rectangle(0,100,0,100)));
    ueMobility.Install(ueNodes);

    // LTE helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();

    // EPC helper for Internet connectivity
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set up default gateway on UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNode->GetObject<Ipv4> ()->GetRoutingProtocol ());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Applications
    uint16_t port = 5000;
    // UDP Server on UE
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Client on eNB (send to UE's IP via EPC)
    UdpClientHelper client(ueIpIface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(simTime));

    // Enable Packet Capture (optional)
    // lteHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}