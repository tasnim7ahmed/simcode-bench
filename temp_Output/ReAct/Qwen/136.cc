#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[])
{
    // Configuration parameters
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);
    uint32_t maxPackets = 20;
    Time simulationDuration = Seconds(10.0);

    // Command line parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("DataRate", "Data rate of the link", dataRate);
    cmd.AddValue("Delay", "Propagation delay of the link", delay);
    cmd.AddValue("PacketSize", "Size of UDP packets", packetSize);
    cmd.AddValue("Interval", "Interval between packets (in ms)", interPacketInterval);
    cmd.AddValue("NumPackets", "Number of packets to send", maxPackets);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install server application on node 1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(simulationDuration);

    // Install client application on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(simulationDuration);

    // Enable ASCII tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));

    // Enable pcap tracing
    pointToPoint.EnablePcapAll("udp-echo");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}