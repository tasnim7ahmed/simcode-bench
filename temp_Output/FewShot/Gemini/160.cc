#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Mesh Network Setup
    NodeContainer meshNodes;
    meshNodes.Create(4);

    PointToPointHelper meshPointToPoint;
    meshPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    meshPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer meshDevices[6];
    meshDevices[0] = meshPointToPoint.Install(meshNodes.Get(0), meshNodes.Get(1));
    meshDevices[1] = meshPointToPoint.Install(meshNodes.Get(0), meshNodes.Get(2));
    meshDevices[2] = meshPointToPoint.Install(meshNodes.Get(0), meshNodes.Get(3));
    meshDevices[3] = meshPointToPoint.Install(meshNodes.Get(1), meshNodes.Get(2));
    meshDevices[4] = meshPointToPoint.Install(meshNodes.Get(1), meshNodes.Get(3));
    meshDevices[5] = meshPointToPoint.Install(meshNodes.Get(2), meshNodes.Get(3));

    InternetStackHelper meshStack;
    meshStack.Install(meshNodes);

    Ipv4AddressHelper meshAddress;
    meshAddress.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer meshInterfaces = meshAddress.Assign(NetDeviceContainer(meshDevices[0],6));

    // Tree Network Setup
    NodeContainer treeNodes;
    treeNodes.Create(4);

    PointToPointHelper treePointToPoint;
    treePointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    treePointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer treeDevices[3];
    treeDevices[0] = treePointToPoint.Install(treeNodes.Get(0), treeNodes.Get(1));
    treeDevices[1] = treePointToPoint.Install(treeNodes.Get(0), treeNodes.Get(2));
    treeDevices[2] = treePointToPoint.Install(treeNodes.Get(2), treeNodes.Get(3));

    InternetStackHelper treeStack;
    treeStack.Install(treeNodes);

    Ipv4AddressHelper treeAddress;
    treeAddress.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer treeInterfaces = treeAddress.Assign(NetDeviceContainer(treeDevices[0],3));

    // UDP Traffic - Mesh
    uint16_t meshPort = 9;
    UdpEchoServerHelper meshEchoServer(meshPort);
    ApplicationContainer meshServerApp = meshEchoServer.Install(meshNodes.Get(3));
    meshServerApp.Start(Seconds(1.0));
    meshServerApp.Stop(Seconds(20.0));

    UdpEchoClientHelper meshEchoClient(meshInterfaces.GetAddress(3), meshPort);
    meshEchoClient.SetAttribute("MaxPackets", UintegerValue(0));
    meshEchoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    meshEchoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer meshClientApp = meshEchoClient.Install(meshNodes.Get(0));
    meshClientApp.Start(Seconds(2.0));
    meshClientApp.Stop(Seconds(20.0));

    // UDP Traffic - Tree
    uint16_t treePort = 9;
    UdpEchoServerHelper treeEchoServer(treePort);
    ApplicationContainer treeServerApp = treeEchoServer.Install(treeNodes.Get(3));
    treeServerApp.Start(Seconds(1.0));
    treeServerApp.Stop(Seconds(20.0));

    UdpEchoClientHelper treeEchoClient(treeInterfaces.GetAddress(3), treePort);
    treeEchoClient.SetAttribute("MaxPackets", UintegerValue(0));
    treeEchoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    treeEchoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer treeClientApp = treeEchoClient.Install(treeNodes.Get(0));
    treeClientApp.Start(Seconds(2.0));
    treeClientApp.Stop(Seconds(20.0));

    // Mobility - Optional
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("0.0"),
                                  "Y", StringValue("0.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=10]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));

    mobility.Install(meshNodes);
    mobility.Install(treeNodes);

    // Flow Monitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Animation
    AnimationInterface anim("mesh-tree.xml");
    anim.SetMaxPktsPerTraceFile(5000000);

    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        anim.UpdateNodeColor(meshNodes.Get(i), 0, 0, 255);
    }
     for (uint32_t i = 0; i < treeNodes.GetN(); ++i) {
        anim.UpdateNodeColor(treeNodes.Get(i), 255, 0, 0);
    }

    // Run Simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Flow Monitor Analysis
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << (double)i->second.rxPackets / (double)i->second.txPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Jitter Sum: " << i->second.jitterSum << "\n";

    }

    Simulator::Destroy();
    return 0;
}