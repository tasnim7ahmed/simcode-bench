#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoClientSimulation");

int main(int argc, char *argv[])
{
    // Log component enable for important modules
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create a single node
    NS_LOG_INFO("Creating node.");
    NodeContainer nodes;
    nodes.Create(1);

    // Setup point-to-point link attributes
    NS_LOG_INFO("Setting up point-to-point channel.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the node
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install internet stack on the node
    NS_LOG_INFO("Installing Internet stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server application
    NS_LOG_INFO("Setting up UDP Echo Server.");
    UdpEchoServerHelper echoServer(9);  // port 9

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Echo Client application
    NS_LOG_INFO("Setting up UDP Echo Client.");
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);  // connect to server's IP and port 9
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));      // send 5 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // packet interval
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));   // packet size in bytes

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("udp-echo-client-server.tr"));
    pointToPoint.EnablePcapAll("udp-echo-client-server");

    // Run the simulation
    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation completed.");
    return 0;
}