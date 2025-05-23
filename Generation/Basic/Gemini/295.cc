#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/point-to-point-epc-helper.h" // For PointToPointEpcHelper
#include "ns3/internet-stack-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. LTE Network Setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>(); // Using PointToPointEpcHelper for EPC

    lteHelper->SetEpcHelper(epcHelper);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Install LTE devices on eNodeB and UE
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes.Get(0));
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes.Get(0));

    // Attach UE to eNodeB
    lteHelper->AttachToClosestEnb(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    Ipv4Address ueAddress = ueIpIface.GetAddress(0); // Get UE's assigned IP address

    // 2. Mobility Setup
    MobilityHelper mobility;

    // eNodeB: Stationary at origin
    mobility.SetMobilityModelType("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    enbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));

    // UE: Random Mobility (RandomWalk2dMobilityModel within a square boundary)
    mobility.SetMobilityModelType("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)));
    mobility.Install(ueNodes);

    // 3. Applications Setup
    // UDP Server on UE
    uint16_t port = 9; // Discard port
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = packetSinkHelper.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on eNodeB
    UdpClientHelper udpClientHelper(ueAddress, port);
    udpClientHelper.SetAttribute("MaxPackets", UintegerValue(4294967295U)); // Send continuously
    udpClientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(100))); // 100ms interval
    udpClientHelper.SetAttribute("PacketSize", UintegerValue(1024));        // 1024 bytes packet size

    ApplicationContainer clientApps = udpClientHelper.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(1.0)); // Start client 1 second into simulation
    clientApps.Stop(Seconds(10.0));

    // 4. Simulation Run
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}