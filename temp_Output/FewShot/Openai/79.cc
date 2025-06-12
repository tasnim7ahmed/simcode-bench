#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomNetworkSimulation");

void
PacketReceiveLogger(Ptr<const Packet> packet, const Address &address)
{
    static std::ofstream recvLog("packet-receptions.log", std::ios_base::app);
    recvLog << Simulator::Now().As(Time::S) << "s\tReceived packet of "
            << packet->GetSize() << " bytes" << std::endl;
}

int main(int argc, char *argv[])
{
    uint32_t metricN1N3 = 10; // Default cost between n1 and n3, configurable via command-line
    CommandLine cmd;
    cmd.AddValue("metricN1N3", "Link metric/cost between n1 and n3", metricN1N3);
    cmd.Parse(argc, argv);

    // Enable necessary logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes: n0, n1, n2, n3
    Ptr<Node> n0 = CreateObject<Node>();
    Ptr<Node> n1 = CreateObject<Node>();
    Ptr<Node> n2 = CreateObject<Node>();
    Ptr<Node> n3 = CreateObject<Node>();
    NodeContainer nodes;
    nodes.Add(n0);
    nodes.Add(n1);
    nodes.Add(n2);
    nodes.Add(n3);

    InternetStackHelper stack;
    stack.Install(nodes);

    // Point-to-point helpers for different links
    // n0--n2: 5 Mbps, 2ms
    PointToPointHelper p2p_5mbps_2ms;
    p2p_5mbps_2ms.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_5mbps_2ms.SetChannelAttribute("Delay", StringValue("2ms"));

    // n1--n2: 5 Mbps, 2ms
    // (reuse above)
    // n1--n3: 1.5 Mbps, 100ms
    PointToPointHelper p2p_1_5mbps_100ms;
    p2p_1_5mbps_100ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_1_5mbps_100ms.SetChannelAttribute("Delay", StringValue("100ms"));

    // n2--n3: 1.5 Mbps, 10ms
    PointToPointHelper p2p_1_5mbps_10ms;
    p2p_1_5mbps_10ms.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_1_5mbps_10ms.SetChannelAttribute("Delay", StringValue("10ms"));

    // Create links
    // n0 <-> n2
    NodeContainer n0n2(n0, n2);
    NetDeviceContainer d0d2 = p2p_5mbps_2ms.Install(n0n2);

    // n1 <-> n2
    NodeContainer n1n2(n1, n2);
    NetDeviceContainer d1d2 = p2p_5mbps_2ms.Install(n1n2);

    // n1 <-> n3
    NodeContainer n1n3(n1, n3);
    NetDeviceContainer d1d3 = p2p_1_5mbps_100ms.Install(n1n3);

    // n2 <-> n3
    NodeContainer n2n3(n2, n3);
    NetDeviceContainer d2d3 = p2p_1_5mbps_10ms.Install(n2n3);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i3 = address.Assign(d1d3);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i2i3 = address.Assign(d2d3);

    // Enable queue tracing
    AsciiTraceHelper ascii;
    p2p_1_5mbps_100ms.EnableAsciiAll(ascii.CreateFileStream("queue-trace-n1n3.tr"));
    p2p_1_5mbps_10ms.EnableAsciiAll(ascii.CreateFileStream("queue-trace-n2n3.tr"));
    p2p_5mbps_2ms.EnableAsciiAll(ascii.CreateFileStream("queue-trace-n0n2_n1n2.tr"));

    // Set metrics; modify the n1-n3 link metric via static routing
    Ptr<Ipv4> ipv4_n1 = n1->GetObject<Ipv4>();
    Ptr<Ipv4> ipv4_n3 = n3->GetObject<Ipv4>();

    // Retrieve interface numbers for static route manipulation
    uint32_t ifIndex_n1_n3 = ipv4_n1->GetInterfaceForDevice(d1d3.Get(0)); // n1 side
    uint32_t ifIndex_n1_n2 = ipv4_n1->GetInterfaceForDevice(d1d2.Get(0)); // n1 side
    uint32_t ifIndex_n3_n1 = ipv4_n3->GetInterfaceForDevice(d1d3.Get(1)); // n3 side

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set link metrics, if supported (otherwise routes are recomputed)
    Ptr<Ipv4StaticRouting> staticRoutingN1 = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ipv4_n1->GetRoutingProtocol());
    if (staticRoutingN1 != nullptr)
    {
        staticRoutingN1->SetInterfaceMetric(ifIndex_n1_n3, metricN1N3);
    }

    Ptr<Ipv4StaticRouting> staticRoutingN3 = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ipv4_n3->GetRoutingProtocol());
    if (staticRoutingN3 != nullptr)
    {
        staticRoutingN3->SetInterfaceMetric(ifIndex_n3_n1, metricN1N3);
    }

    // Re-populate routing tables so that the new metrics are used
    Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

    // Install UDP Echo Server on n1
    uint16_t echoPort = 8080;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(n1);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Add receive logging for n1's UDP port
    Ptr<Socket> recvSocket = Socket::CreateSocket(n1, UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), echoPort);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&PacketReceiveLogger));

    // Install UDP Echo Client on n3 (to n1)
    UdpEchoClientHelper echoClient(i1i2.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApp = echoClient.Install(n3);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(18.0));

    // Enable pcap tracing (optional)
    p2p_1_5mbps_100ms.EnablePcapAll("n1n3");
    p2p_1_5mbps_10ms.EnablePcapAll("n2n3");
    p2p_5mbps_2ms.EnablePcapAll("n0n2_n1n2");

    // Log to simulation results file
    std::ofstream simLog("simulation-results.log");
    simLog << "NS-3 simulation with n0, n1, n2, n3" << std::endl;
    simLog << "Link n1-n3 metric: " << metricN1N3 << std::endl;
    simLog << "UDP traffic from n3 to n1, port " << echoPort << std::endl;
    simLog << "Simulation time: 20s" << std::endl;
    simLog.close();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}