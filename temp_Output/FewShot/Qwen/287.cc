#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation duration
    double simTime = 10.0;

    // Create a single gNB and two UEs
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(2);

    // Concatenate all nodes for mobility and internet stack installation
    NodeContainer allNodes = NodeContainer(enbNodes, ueNodes);

    // Install Mobility model - Random positions within 50x50 area
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                              "Distance", DoubleValue(1.0));
    mobility.Install(ueNodes);

    // Set gNB to be stationary at center (25,25)
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(25.0, 25.0, 0.0));
    mobility.SetPositionAllocator(enbPositionAlloc);
    mobility.Install(enbNodes);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Configure mmWave helper
    mmwave::MmWaveHelper mmwaveHelper;
    mmwaveHelper.Initialize();

    // Install devices
    NetDeviceContainer enbDevs = mmwaveHelper.InstallEnbDevice(enbNodes, allNodes);
    NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes, allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipAllocator;
    ipAllocator.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIfs = ipAllocator.Assign(enbDevs);
    ipAllocator.NewNetwork();
    Ipv4InterfaceContainer ueIfs = ipAllocator.Assign(ueDevs);

    // Attach UEs to the first gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        mmwaveHelper.Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Setup UDP Server on UE 1 (IP: ueIfs.GetAddress(1)) on port 5001
    uint16_t udpPort = 5001;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Setup UDP Client on UE 0, sending to UE 1's IP address
    UdpClientHelper client(ueIfs.GetAddress(1), udpPort);
    client.SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited packets
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(simTime));

    // Enable PCAP tracing
    mmwaveHelper.EnableTraces();
    point_to_point::PointToPointHelper p2p;
    p2p.EnablePcapAll("mmwave-5g-ue-gnb");

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}