#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/nr-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create Nodes: gNodeB and UE
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Configure mobility for UE (Random Walk)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(ueNodes);

    // Configure mobility for gNodeB (fixed position)
    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();
    gnbPositionAlloc->Add(Vector(50.0, 50.0, 0.0)); // Example fixed position
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.SetPositionAllocator(gnbPositionAlloc);
    gnbMobility.Install(gnbNodes);

    // Create NR devices
    NetDeviceContainer gnbDevs;
    NetDeviceContainer ueDevs;
    NrHelper nrHelper;
    gnbDevs = nrHelper.InstallGnbDevice(gnbNodes);
    ueDevs = nrHelper.InstallUeDevice(ueNodes);

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(gnbNodes);
    stack.Install(ueNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueInterfaces = address.Assign(ueDevs);
    Ipv4InterfaceContainer gnbInterfaces = address.Assign(gnbDevs);

    // Install TCP server on gNodeB
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(gnbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install TCP client on UE
    UdpClientHelper client(gnbInterfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(5));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}