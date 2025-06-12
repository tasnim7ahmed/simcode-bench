#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUdpEcho");

int main(int argc, char *argv[]) {
    // Log component level
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create central hub node and 4 peripheral nodes
    NodeContainer hub;
    hub.Create(1);

    NodeContainer peripherals;
    peripherals.Create(4);

    // Connect each peripheral to the hub using point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer devicesHub[4];
    NetDeviceContainer devicesPeriph[4];

    for (uint32_t i = 0; i < 4; ++i) {
        devicesHub[i] = p2p.Install(hub.Get(0), peripherals.Get(i));
        devicesPeriph[i] = devicesHub[i]; // Same container, different perspective
    }

    // Install Internet stack
    InternetStackHelper stack;
    hub.Get(0)->SetAttribute("Ipv4", PointerValue(CreateObject<Ipv4L3Protocol>()));
    stack.Install(peripherals);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesHub[4];
    Ipv4InterfaceContainer interfacesPeriph[4];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfacesHub[i] = address.Assign(devicesHub[i]);
        interfacesPeriph[i] = interfacesHub[i]; // Same assignment
    }

    // Server on the hub node
    UdpEchoServerHelper echoServer(9); // port 9
    ApplicationContainer serverApps = echoServer.Install(hub.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Clients on peripheral nodes
    for (uint32_t i = 0; i < 4; ++i) {
        UdpEchoClientHelper echoClient(interfacesHub[i].GetAddress(0), 9); // connect to hub
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(peripherals.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable pcap tracing
    p2p.EnablePcapAll("star_topology_udp_echo");

    // Set up FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Simulation run
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    // Output flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::cout << "\nFlowMonitor results:\n";
    for (const auto &elem : stats) {
        FlowId id = elem.first;
        FlowMonitor::FlowStats st = elem.second;

        if (classifier != nullptr) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);
            std::cout << "Flow ID: " << id << " Src Addr: " << t.sourceAddress << ":" << t.sourcePort
                      << " Dst Addr: " << t.destinationAddress << ":" << t.destinationPort << "\n";
        }

        std::cout << "  Tx Packets: " << st.txPackets << "\n";
        std::cout << "  Rx Packets: " << st.rxPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << ((double)st.rxPackets / (double)st.txPackets) * 100.0 << "%\n";
        std::cout << "  Average Delay: " << st.delaySum.GetSeconds() / st.rxPackets << "s\n";
        std::cout << "  Throughput: " << st.rxBytes * 8.0 / (st.timeLastRxPacket.GetSeconds() - st.timeFirstTxPacket.GetSeconds()) / 1000000.0 << " Mbps\n";
    }

    return 0;
}