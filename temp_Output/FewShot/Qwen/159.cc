#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for UDP echo applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create all 7 nodes
    NodeContainer starNodes;
    starNodes.Create(4);  // Node 0 (central), Nodes 1-3 (peripheral)
    NodeContainer busNodes;
    busNodes.Create(3);   // Nodes 4, 5, 6

    // Combine all nodes into one container for internet stack installation
    NodeContainer allNodes = starNodes;
    allNodes.Add(busNodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Point-to-point helper for links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Star topology: connect Node 0 to each of the peripheral nodes
    NetDeviceContainer starDevices[3];
    Ipv4InterfaceContainer starInterfaces[3];
    for (int i = 0; i < 3; ++i) {
        NodeContainer pair(starNodes.Get(0), starNodes.Get(i + 1));
        starDevices[i] = p2p.Install(pair);

        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        starInterfaces[i] = address.Assign(starDevices[i]);
    }

    // Bus topology: connect Nodes 4 -> 5 -> 6 sequentially
    NetDeviceContainer busDevices[2];
    Ipv4InterfaceContainer busInterfaces[2];
    for (int i = 0; i < 2; ++i) {
        NodeContainer pair(busNodes.Get(i), busNodes.Get(i + 1));
        busDevices[i] = p2p.Install(pair);

        Ipv4AddressHelper address;
        std::ostringstream subnet;
        subnet << "10.2." << (i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        busInterfaces[i] = address.Assign(busDevices[i]);
    }

    // Set up UDP Echo Server on Node 0 (central node in star topology)
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(starNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Set up UDP Echo Clients on Nodes 4 and 5 (bus topology)
    ApplicationContainer clientApps;

    // Client on Node 4 sending to Node 0
    UdpEchoClientHelper clientHelper1(starInterfaces[0].GetAddress(0), port);
    clientHelper1.SetAttribute("MaxPackets", UintegerValue(10));
    clientHelper1.SetAttribute("Interval", TimeValue(Seconds(2.0)));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(512));
    clientApps = clientHelper1.Install(busNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Client on Node 5 sending to Node 0
    UdpEchoClientHelper clientHelper2(starInterfaces[0].GetAddress(0), port);
    clientHelper2.SetAttribute("MaxPackets", UintegerValue(10));
    clientHelper2.SetAttribute("Interval", TimeValue(Seconds(3.0)));
    clientHelper2.SetAttribute("PacketSize", UintegerValue(512));
    clientApps = clientHelper2.Install(busNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Install FlowMonitor to track performance metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.Install(allNodes);

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Output flow statistics
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowId id = iter->first;
        FlowMonitor::FlowStats s = iter->second;
        std::cout << "Flow ID: " << id << "  Packets Dropped: " << s.lostPackets << "  Delay: " << s.delaySum.GetSeconds() << "s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}