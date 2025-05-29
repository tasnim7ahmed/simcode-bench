#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("LteEnbRrc", LOG_LEVEL_INFO);
    LogComponentEnable("LteUeRrc", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: two eNodeBs and one UE
    NodeContainer enbNodes;
    enbNodes.Create(2);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Configure LTE helper
    LteHelper lteHelper;
    lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(500));
    lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18000));
    NetDeviceContainer enbDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper.InstallUeDevice(ueNodes);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", DoubleValue(0.0),
                                  "Y", DoubleValue(0.0),
                                  "Z", DoubleValue(1.5),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100, 100, -100, 100)));
    mobility.Install(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    Ipv4InterfaceContainer enbIpIface;

    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    ueIpIface = ip.Assign(ueDevs);
    enbIpIface = ip.Assign(enbDevs);

    // Attach UE to eNodeB
    lteHelper.Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Set handover algorithm
    lteHelper.AddX2Interface(enbNodes);
    lteHelper.SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");
    lteHelper.SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(1.0));
    lteHelper.SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(40)));

    // UDP server on eNodeB 0
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client on UE
    UdpClientHelper client(enbIpIface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}