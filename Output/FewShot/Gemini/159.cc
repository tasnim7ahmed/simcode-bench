#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Create nodes
    NodeContainer starNodes;
    starNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(3);

    // Star topology
    PointToPointHelper starPointToPoint;
    starPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    starPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices;
    for (int i = 1; i < 4; ++i) {
        NetDeviceContainer link = starPointToPoint.Install(starNodes.Get(0), starNodes.Get(i));
        starDevices.Add(link.Get(0));
        starDevices.Add(link.Get(1));
    }

    // Bus topology
    PointToPointHelper busPointToPoint;
    busPointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    busPointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer busDevices;
    for (int i = 0; i < 2; ++i) {
        NetDeviceContainer link = busPointToPoint.Install(busNodes.Get(i), busNodes.Get(i + 1));
        busDevices.Add(link.Get(0));
        busDevices.Add(link.Get(1));
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll(starNodes);
    stack.InstallAll(busNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer starInterfaces = address.Assign(starDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer busInterfaces = address.Assign(busDevices);

    // Set up UDP echo server on Node 0
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(starNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP echo clients on Nodes 4 and 5
    UdpEchoClientHelper echoClient1(busInterfaces.GetAddress(1), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient1.Install(busNodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient2(busInterfaces.GetAddress(3), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp2 = echoClient2.Install(busNodes.Get(1));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(20.0));

    // FlowMonitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Run the simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Analyze flow monitor data
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
    }

    Simulator::Destroy();
    return 0;
}