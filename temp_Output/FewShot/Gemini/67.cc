#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("DumbbellSimulation");

void QueueSizeTrace(std::string filename, std::string queueDiscPath) {
    AsciiTraceHelper ascii;
    Ptr<QueueDisc> queueDisc = DynamicCast<QueueDisc>(GetObject<QueueDisc>(queueDiscPath));
    if (queueDisc) {
        ascii.EnableWithCallback(filename, queueDiscPath, "queueSize",
                                MakeCallback(&QueueDisc::ReportQueueSize));
    } else {
        NS_LOG_ERROR("QueueDisc not found at path: " << queueDiscPath);
    }
}

void CwndTracer(std::string filename) {
    AsciiTraceHelper ascii;
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/CongestionWindow",
                                 MakeCallback(&ascii.TraceCwnd));
    ascii.EnableAsciiAll(filename);
}

void experiment(std::string queueDiscName, std::string queueSizeTraceFile, std::string cwndTraceFile) {
    // Set simulation parameters
    double simulationTime = 10.0;
    std::string dataRate = "10Mbps";
    std::string delay = "2ms";
    uint16_t port = 50000;
    uint32_t packetSize = 1024;
    uint32_t numSenders = 7;

    // Create nodes
    NodeContainer leftNodes, rightNodes, routerNodes;
    leftNodes.Create(numSenders);
    rightNodes.Create(1);
    routerNodes.Create(2);

    // Create point-to-point helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    // Install link between senders and left router
    NetDeviceContainer leftRouterDevices, senderDevices;
    for (uint32_t i = 0; i < numSenders; ++i) {
        NetDeviceContainer linkDevices = pointToPoint.Install(leftNodes.Get(i), routerNodes.Get(0));
        senderDevices.Add(linkDevices.Get(0));
        leftRouterDevices.Add(linkDevices.Get(1));
    }

    // Install link between receiver and right router
    NetDeviceContainer rightRouterDevices, receiverDevices;
    NetDeviceContainer linkDevices = pointToPoint.Install(rightNodes.Get(0), routerNodes.Get(1));
    receiverDevices.Add(linkDevices.Get(0));
    rightRouterDevices.Add(linkDevices.Get(1));

    // Install link between routers
    NetDeviceContainer routerRouterDevices = pointToPoint.Install(routerNodes.Get(0), routerNodes.Get(1));

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer senderInterfaces;
    for (uint32_t i = 0; i < numSenders; ++i) {
        senderInterfaces.Add(address.AssignOne(senderDevices.Get(i)));
    }
    Ipv4InterfaceContainer leftRouterInterface = address.Assign(leftRouterDevices);
    address.NewNetwork();

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer receiverInterface = address.Assign(receiverDevices);
    Ipv4InterfaceContainer rightRouterInterface = address.Assign(rightRouterDevices);
    address.NewNetwork();

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer routerRouterInterface = address.Assign(routerRouterDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure queue disc
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueDiscName);
    QueueDiscContainer queueDiscs = tch.Install(routerRouterDevices);

    // Set up TCP client and server
    TcpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(rightNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    TcpEchoClientHelper echoClient(routerRouterInterface.GetAddress(1), port);
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));

    ApplicationContainer clientApps[numSenders];
    for (uint32_t i = 0; i < numSenders; ++i) {
        clientApps[i] = echoClient.Install(leftNodes.Get(i));
        clientApps[i].Start(Seconds(2.0));
        clientApps[i].Stop(Seconds(simulationTime));
    }

     // Set up UDP client and server
    UdpEchoServerHelper udpEchoServer(port+1);
    ApplicationContainer udpServerApp = udpEchoServer.Install(rightNodes.Get(0));
    udpServerApp.Start(Seconds(1.0));
    udpServerApp.Stop(Seconds(simulationTime));

    UdpEchoClientHelper udpEchoClient(routerRouterInterface.GetAddress(1), port+1);
    udpEchoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    udpEchoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    udpEchoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));

    ApplicationContainer udpClientApps[numSenders];
    for (uint32_t i = 0; i < numSenders; ++i) {
        udpClientApps[i] = udpEchoClient.Install(leftNodes.Get(i));
        udpClientApps[i].Start(Seconds(2.0));
        udpClientApps[i].Stop(Seconds(simulationTime));
    }

    // Configure Tracing
    QueueSizeTrace(queueSizeTraceFile, "/NodeList/2/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Disc");
    CwndTracer(cwndTraceFile);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("DumbbellSimulation", LOG_LEVEL_INFO);
    // SeedManager::SetSeed(1);
    // Run the experiment with COBALT
    experiment("ns3::CobaltQueueDisc", "cobalt_queue_size.txt", "cobalt_cwnd.txt");

    // Run the experiment with CoDel
    experiment("ns3::CoDelQueueDisc", "codel_queue_size.txt", "codel_cwnd.txt");

    return 0;
}