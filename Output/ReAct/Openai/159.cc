#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Node creation
    NodeContainer allNodes;
    allNodes.Create(7);

    // Star topology: central node 0, peripherals 1,2,3
    NodeContainer starCenter = NodeContainer(allNodes.Get(0));
    NodeContainer starPeripherals;
    for (uint32_t i = 1; i <= 3; ++i) {
        starPeripherals.Add(allNodes.Get(i));
    }

    std::vector<NodeContainer> starLinks;
    for (uint32_t i = 1; i <= 3; ++i) {
        starLinks.push_back(NodeContainer(allNodes.Get(0), allNodes.Get(i)));
    }

    // Bus topology: 4-5-6 in a chain
    NodeContainer busNodes;
    for (uint32_t i = 4; i <= 6; ++i) {
        busNodes.Add(allNodes.Get(i));
    }
    std::vector<NodeContainer> busLinks;
    busLinks.push_back(NodeContainer(busNodes.Get(0), busNodes.Get(1))); // 4-5
    busLinks.push_back(NodeContainer(busNodes.Get(1), busNodes.Get(2))); // 5-6

    // Point-to-point configuration
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install star links
    NetDeviceContainer starDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        starDevices[i] = p2p.Install(starLinks[i]);
    }

    // Install bus links
    NetDeviceContainer busDevices[2];
    for (uint32_t i = 0; i < 2; ++i) {
        busDevices[i] = p2p.Install(busLinks[i]);
    }

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> starIfaces;
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        starIfaces.push_back(address.Assign(starDevices[i]));
    }

    std::vector<Ipv4InterfaceContainer> busIfaces;
    for (uint32_t i = 0; i < 2; ++i) {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        busIfaces.push_back(address.Assign(busDevices[i]));
    }

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo server on Node 0
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(allNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // UDP Echo clients on Nodes 4 and 5 (from bus topology)
    uint32_t packetSize = 1024;
    uint32_t nPackets = 5;
    Time interPacketInterval = Seconds(1.0);

    // Get Node 0's IP address on its first p2p link (10.1.1.1)
    Ipv4Address serverIp = starIfaces[0].GetAddress(0);

    UdpEchoClientHelper echoClient(serverIp, echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    clientApps.Add(echoClient.Install(allNodes.Get(4))); // Node 4
    clientApps.Add(echoClient.Install(allNodes.Get(5))); // Node 5

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("hybrid-star-bus-flowmonitor.xml", true, true);

    Simulator::Destroy();
    return 0;
}