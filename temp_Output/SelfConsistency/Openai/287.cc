#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWaveSimpleUdpExample");

int main(int argc, char *argv[])
{
    // Set simulation time and random seed
    double simTime = 10.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: one gNB, two UEs
    NodeContainer gNbNodes, ueNodes;
    gNbNodes.Create(1);
    ueNodes.Create(2);

    // Set mobility
    MobilityHelper mobility;
    // gNB is stationary at center
    Ptr<ListPositionAllocator> gNbPos = CreateObject<ListPositionAllocator>();
    gNbPos->Add(Vector(25.0, 25.0, 3.0));
    mobility.SetPositionAllocator(gNbPos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    // UEs move randomly in the area
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", TimeValue(Seconds(1.0)),
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                              "Bounds", RectangleValue(Rectangle(0.0, 50.0, 0.0, 50.0)));
    mobility.Install(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(gNbNodes);
    internet.Install(ueNodes);

    // mmWave setup
    mmwave::MmWaveHelper mmwaveHelper;
    mmwaveHelper.SetAttribute("Scheduler", StringValue("ns3::MmWaveFlexTtiMacScheduler"));
    mmwaveHelper.Initialize();

    // Create Devices
    NetDeviceContainer gNbDevs = mmwaveHelper.InstallEnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes);

    // Attach UEs to the gNB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        mmwaveHelper.Attach(ueDevs.Get(i), gNbDevs.Get(0));
    }

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("7.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer ueIfaces = ipv4h.Assign(ueDevs);
    Ipv4InterfaceContainer gNbIfaces = ipv4h.Assign(gNbDevs);

    // Set default route for UEs
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(i)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(gNbIfaces.GetAddress(0), 1);
    }

    // Setup application: UE 0 = UDP server, UE 1 = UDP client

    // UDP Server on UE 0
    uint16_t serverPort = 5001;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(0.1));
    serverApp.Stop(Seconds(simTime));

    // UDP Client on UE 1
    uint32_t pktSize = 1024;
    double interval = 0.01; // 10ms
    UdpClientHelper udpClient(ueIfaces.GetAddress(0), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(pktSize));
    ApplicationContainer clientApp = udpClient.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(0.2));
    clientApp.Stop(Seconds(simTime));

    // Enable PCAP tracing
    mmwaveHelper.EnableTraces();
    mmwaveHelper.EnablePcapAll("mmwave-5g-simple");

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}