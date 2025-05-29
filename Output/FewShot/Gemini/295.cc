#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create LTE helper
    LteHelper lteHelper;

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Configure eNodeB
    lteHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(100));
    lteHelper.SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18100));

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    ueNodes.Add(enbNodes.Get(0));
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;

    // Attach the UE to the eNodeB
    lteHelper.Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    ueIpIface = ip.Assign(NetDeviceContainer(ueLteDevs.Get(0),enbLteDevs.Get(0)));
    Ipv4Address ueAddress = ueIpIface.GetAddress(0);
    Ipv4Address enbAddress = ueIpIface.GetAddress(1);

    // Set mobility for eNodeB (stationary)
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.Install(enbNodes);

    // Set mobility for UE (random)
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                     "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"),
                                     "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue(Rectangle(0, 10, 0, 10)));
    ueMobility.Install(ueNodes);

    // Install UDP server on UE
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP client on eNodeB
    UdpClientHelper client(ueAddress, port);
    client.SetAttribute("MaxPackets", UintegerValue(100));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(enbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();
    return 0;
}