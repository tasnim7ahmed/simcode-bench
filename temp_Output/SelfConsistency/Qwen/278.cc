#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHttpSimulation");

int main(int argc, char *argv[])
{
    // Log components
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("HttpClientApplication", LOG_LEVEL_INFO);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // LTE helper setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseRlcSm", BooleanValue(true));

    // Install LTE devices
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the first eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set up UDP server (simulate HTTP port 80)
    UdpServerHelper udpServer(80);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client (HTTP-like behavior)
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), 80);
    udpClient.SetAttribute("MaxPackets", UintegerValue(5));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Mobility model for UE: Random Walk in 50x50 area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);

    // Static position for eNodeB
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    // Enable LTE logging
    lteHelper->EnableTraces();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}