#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-routing-helper.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpRttExample");

void TraceRtt(Ptr<const TcpSocketState> state)
{
    // Print the RTT value
    Time rtt = state->m_rtt;
    NS_LOG_INFO("RTT: " << rtt.GetSeconds() << " seconds");
}

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("TcpRttExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link between nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install internet stack (IP, TCP, etc.)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to nodes
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipInterfaces = ipv4.Assign(devices);

    // Set up TCP server on node 1
    uint16_t port = 8080;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on node 0
    TcpClientHelper tcpClient(ipInterfaces.GetAddress(1), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    tcpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Trace RTT for TCP connection
    Config::Connect("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback(&TraceRtt));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
