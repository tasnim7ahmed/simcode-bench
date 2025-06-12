#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("LteUdpSimulation", LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 10.0;
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(200);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Setup mobility for UE (random movement in 100x100 area)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Distance", DoubleValue(5.0));
    mobility.Install(ueNodes);

    // Setup LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseRlcSm", BooleanValue(true));

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install IP stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address ueAddr = ueIpIface.GetAddress(0, 1);

    // Setup applications
    UdpServerHelper server(5000);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(ueAddr, 5000);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    // Enable LTE traces
    lteHelper->EnableTraces();

    // Schedule simulation stop
    Simulator::Stop(Seconds(simTime));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}