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

    // Create the eNodeB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Setup mobility for UE (random walk)
    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                    "X", StringValue("0.0"),
                                    "Y", StringValue("0.0"),
                                    "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobilityUe.Install(ueNodes);

    // Setup LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address ueAddr = ueIpIface.GetAddress(0, 0);

    // Attach UEs to eNodeBs
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Setup UDP server on UE
    UdpServerHelper server(9); // port 9
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP client on eNodeB
    UdpClientHelper client(ueAddr, 9);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(10.0));

    // Start simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}