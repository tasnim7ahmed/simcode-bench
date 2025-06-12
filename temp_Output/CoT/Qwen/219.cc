#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoServer and UdpEchoClient
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoSimulation", LOG_LEVEL_INFO);

    // Create 2 nodes: client and server
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UdpEchoServer on server node (node 1)
    UdpEchoServerHelper echoServer(9); // port number 9
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UdpEchoClient on client node (node 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9); // connect to server's IP and port
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));     // send up to 10 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // packet interval
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));   // packet size

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up tracing
    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("udp-echo-simulation.tr"));
    pointToPoint.EnablePcapAll("udp-echo-simulation");

    NS_LOG_INFO("Starting simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    return 0;
}