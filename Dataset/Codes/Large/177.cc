#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpCongestionControlExample");

int main(int argc, char *argv[])
{
    // Set up logging
    LogComponentEnable("TcpCongestionControlExample", LOG_LEVEL_INFO);

    // Simulation time
    double simulationTime = 10.0; // seconds

    // Create nodes for the dumbbell topology
    NodeContainer leftNodes, rightNodes, routerNodes;
    leftNodes.Create(2);   // Two TCP sources
    rightNodes.Create(2);  // Two TCP sinks
    routerNodes.Create(2); // Routers (bottleneck)

    // Create point-to-point link helpers
    PointToPointHelper pointToPointLeft, pointToPointRight, pointToPointRouter;
    pointToPointLeft.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPointLeft.SetChannelAttribute("Delay", StringValue("5ms"));

    pointToPointRight.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPointRight.SetChannelAttribute("Delay", StringValue("5ms"));

    pointToPointRouter.SetDeviceAttribute("DataRate", StringValue("2Mbps")); // Bottleneck link
    pointToPointRouter.SetChannelAttribute("Delay", StringValue("20ms"));

    // Install devices on the nodes
    NetDeviceContainer leftDevices1 = pointToPointLeft.Install(leftNodes.Get(0), routerNodes.Get(0));
    NetDeviceContainer leftDevices2 = pointToPointLeft.Install(leftNodes.Get(1), routerNodes.Get(0));

    NetDeviceContainer rightDevices1 = pointToPointRight.Install(rightNodes.Get(0), routerNodes.Get(1));
    NetDeviceContainer rightDevices2 = pointToPointRight.Install(rightNodes.Get(1), routerNodes.Get(1));

    NetDeviceContainer routerDevices = pointToPointRouter.Install(routerNodes);

    // Install internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(leftNodes);
    internet.Install(rightNodes);
    internet.Install(routerNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces1 = ipv4.Assign(leftDevices1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer leftInterfaces2 = ipv4.Assign(leftDevices2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces1 = ipv4.Assign(rightDevices1);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer rightInterfaces2 = ipv4.Assign(rightDevices2);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    ipv4.Assign(routerDevices);

    // Set up TCP applications
    uint16_t port = 8080;
    Address sinkAddress1(InetSocketAddress(rightInterfaces1.GetAddress(0), port));
    PacketSinkHelper sinkHelper1("ns3::TcpSocketFactory", sinkAddress1);
    ApplicationContainer sinkApp1 = sinkHelper1.Install(rightNodes.Get(0));

    Address sinkAddress2(InetSocketAddress(rightInterfaces2.GetAddress(0), port));
    PacketSinkHelper sinkHelper2("ns3::TcpSocketFactory", sinkAddress2);
    ApplicationContainer sinkApp2 = sinkHelper2.Install(rightNodes.Get(1));

    sinkApp1.Start(Seconds(0.0));
    sinkApp1.Stop(Seconds(simulationTime));
    sinkApp2.Start(Seconds(0.0));
    sinkApp2.Stop(Seconds(simulationTime));

    // Set TCP socket types for the flows
    TypeId tcpRenoTypeId = TypeId::LookupByName("ns3::TcpReno");
    TypeId tcpCubicTypeId = TypeId::LookupByName("ns3::TcpCubic");

    // Create TCP flow 1 using TCP Reno
    Ptr<Socket> tcpRenoSocket = Socket::CreateSocket(leftNodes.Get(0), tcpRenoTypeId);
    Ptr<TcpSocketBase> tcpReno = DynamicCast<TcpSocketBase>(tcpRenoSocket);
    tcpReno->Bind();
    tcpReno->Connect(sinkAddress1);

    // Create TCP flow 2 using TCP Cubic
    Ptr<Socket> tcpCubicSocket = Socket::CreateSocket(leftNodes.Get(1), tcpCubicTypeId);
    Ptr<TcpSocketBase> tcpCubic = DynamicCast<TcpSocketBase>(tcpCubicSocket);
    tcpCubic->Bind();
    tcpCubic->Connect(sinkAddress2);

    // Set up the on-off application to simulate traffic generation
    OnOffHelper onOffTcpReno("ns3::TcpSocketFactory", sinkAddress1);
    onOffTcpReno.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffTcpReno.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffTcpReno.SetAttribute("DataRate", StringValue("5Mbps"));
    onOffTcpReno.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer tcpRenoApp = onOffTcpReno.Install(leftNodes.Get(0));
    tcpRenoApp.Start(Seconds(1.0));
    tcpRenoApp.Stop(Seconds(simulationTime));

    OnOffHelper onOffTcpCubic("ns3::TcpSocketFactory", sinkAddress2);
    onOffTcpCubic.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffTcpCubic.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onOffTcpCubic.SetAttribute("DataRate", StringValue("5Mbps"));
    onOffTcpCubic.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer tcpCubicApp = onOffTcpCubic.Install(leftNodes.Get(1));
    tcpCubicApp.Start(Seconds(1.0));
    tcpCubicApp.Stop(Seconds(simulationTime));

    // Set up NetAnim for visualization
    AnimationInterface anim("tcp_congestion_control_netanim.xml");

    // Set constant positions for NetAnim visualization
    anim.SetConstantPosition(leftNodes.Get(0), 1.0, 2.0);  // TCP Reno source
    anim.SetConstantPosition(leftNodes.Get(1), 1.0, 3.0);  // TCP Cubic source
    anim.SetConstantPosition(rightNodes.Get(0), 3.0, 2.0); // TCP Reno sink
    anim.SetConstantPosition(rightNodes.Get(1), 3.0, 3.0); // TCP Cubic sink
    anim.SetConstantPosition(routerNodes.Get(0), 2.0, 2.5); // Router 1
    anim.SetConstantPosition(routerNodes.Get(1), 2.0, 3.5); // Router 2

    // Enable packet metadata in NetAnim
    anim.EnablePacketMetadata(true);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

