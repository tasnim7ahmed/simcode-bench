#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/udp-server.h"
#include "ns3/udp-client.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create a single eNodeB and UE node
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Setup mobility for UE with random movement
    MobilityHelper mobilityUe;
    mobilityUe.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                    "X", StringValue("0.0"),
                                    "Y", StringValue("0.0"),
                                    "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=100]"));
    mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobilityUe.Install(ueNodes);

    // Setup static position for eNodeB
    MobilityHelper mobilityEnb;
    mobilityEnb.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(0.0),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(100.0),
                                     "DeltaY", DoubleValue(0.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UE to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer enbIfaces = ipv4.Assign(enbDevs);
    Ipv4InterfaceContainer ueIfaces = ipv4.Assign(ueDevs);

    // Set up UDP server on UE
    UdpServerHelper server(9); // port 9
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client on eNodeB
    UdpClientHelper client(ueIfaces.GetAddress(0), 9); // connect to UE's IP and port 9
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // unlimited packets
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}