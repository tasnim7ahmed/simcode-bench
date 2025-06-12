#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("LteUdpSimulation", LOG_LEVEL_INFO);

    // Create the nodes
    NodeContainer enbNode;
    enbNode.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set pathloss model
    lteHelper->SetPathlossModelType(TypeId::LookupByName("ns3::FriisSpectrumPropagationLossModel"));

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNode);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on UEs and GW
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(epcHelper->GetPgwNode());

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Set up UDP server on UE 1 (index 1)
    uint16_t udpPort = 9;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client on UE 0 (index 0) sending to UE 1's IP address
    UdpClientHelper client(ueIpIface.GetAddress(1), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(2));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Mobility setup: place nodes in a grid layout
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNode);
    mobility.Install(ueNodes);

    // Set positions
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // eNodeB
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));  // UE 0
    positionAlloc->Add(Vector(200.0, 0.0, 0.0));  // UE 1

    MobilityHelper mobilityGrid;
    mobilityGrid.SetPositionAllocator(positionAlloc);
    mobilityGrid.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityGrid.Install(enbNode.Get(0));
    mobilityGrid.Install(ueNodes.Get(0));
    mobilityGrid.Install(ueNodes.Get(1));

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}