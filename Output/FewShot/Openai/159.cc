#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for the UDP applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer starCenter;
    starCenter.Create(1); // Node 0

    NodeContainer starPeripherals;
    starPeripherals.Create(3); // Nodes 1,2,3

    NodeContainer busNodes;
    busNodes.Create(3); // Nodes 4,5,6

    // Create star topology: Node0 <-> Node1/2/3
    PointToPointHelper p2pStar;
    p2pStar.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devicesStar[3];
    for (uint32_t i = 0; i < 3; ++i) {
        NodeContainer pair(starCenter.Get(0), starPeripherals.Get(i));
        devicesStar[i] = p2pStar.Install(pair);
    }

    // Create bus topology: Node4 <-> Node5 <-> Node6
    PointToPointHelper p2pBus;
    p2pBus.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBus.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devicesBus01 = p2pBus.Install(NodeContainer(busNodes.Get(0), busNodes.Get(1))); // 4 <-> 5
    NetDeviceContainer devicesBus12 = p2pBus.Install(NodeContainer(busNodes.Get(1), busNodes.Get(2))); // 5 <-> 6

    // Install Internet stack
    NodeContainer allNodes;
    allNodes.Add(starCenter);
    allNodes.Add(starPeripherals);
    allNodes.Add(busNodes);

    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;

    // Star links
    Ipv4InterfaceContainer ifaceStar[3];
    address.SetBase("10.1.1.0", "255.255.255.0");
    ifaceStar[0] = address.Assign(devicesStar[0]);
    address.NewNetwork();
    ifaceStar[1] = address.Assign(devicesStar[1]);
    address.NewNetwork();
    ifaceStar[2] = address.Assign(devicesStar[2]);
    address.NewNetwork();

    // Bus links
    Ipv4InterfaceContainer ifaceBus01, ifaceBus12;

    address.SetBase("10.2.1.0", "255.255.255.0");
    ifaceBus01 = address.Assign(devicesBus01);
    address.NewNetwork();
    ifaceBus12 = address.Assign(devicesBus12);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server on Node 0 (star center)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(starCenter.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // UDP Echo Clients on Nodes 4 and 5 (bus), targeting Node 0
    Ipv4Address serverAddress = ifaceStar[0].GetAddress(0);
    UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp4 = echoClient.Install(busNodes.Get(0)); // Node 4
    clientApp4.Start(Seconds(2.0));
    clientApp4.Stop(Seconds(19.0));

    ApplicationContainer clientApp5 = echoClient.Install(busNodes.Get(1)); // Node 5
    clientApp5.Start(Seconds(3.0));
    clientApp5.Stop(Seconds(19.0));

    // FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->SerializeToXmlFile("hybrid-topology-flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}