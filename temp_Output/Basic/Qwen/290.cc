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

    // Create the simulation objects
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    // Create nodes: 1 eNodeB and 1 UE
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to the eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=100.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(ueNodes);

    // Enable tracing for PHY, MAC, RLC layers
    lteHelper->EnableTraces();

    // Set up applications
    uint16_t dlPort = 5000;

    // UDP Server on UE
    UdpServerHelper server(dlPort);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on eNodeB
    UdpClientHelper client(ueIpIface.GetAddress(0), dlPort);
    client.SetAttribute("MaxPackets", UintegerValue(49)); // ~10 seconds at 200ms interval
    client.SetAttribute("Interval", TimeValue(MilliSeconds(200)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(10.0));

    // Simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}