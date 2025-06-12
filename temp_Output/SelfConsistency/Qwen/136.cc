#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoClientServerSimulation");

int main(int argc, char *argv[])
{
    // Default configuration values
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";
    uint32_t packetSize = 1024;
    uint32_t numberOfPackets = 10;
    Time interPacketInterval = Seconds(1.0);

    // Command line arguments parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Data rate of the point-to-point link", dataRate);
    cmd.AddValue("delay", "Propagation delay of the point-to-point link", delay);
    cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
    cmd.AddValue("numberOfPackets", "Number of packets to send", numberOfPackets);
    cmd.AddValue("interPacketInterval", "Interval between packets (in seconds)", interPacketInterval);
    cmd.Parse(argc, argv);

    // Enable logging for UdpEchoClient and UdpEchoServer applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create two nodes
    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link
    NS_LOG_INFO("Setting up point-to-point link.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on all nodes
    NS_LOG_INFO("Installing internet stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP Echo Server on node 1
    NS_LOG_INFO("Installing UDP echo server application on node 1.");
    UdpEchoServerHelper echoServer(9); // port number

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Echo Client on node 0
    NS_LOG_INFO("Installing UDP echo client application on node 0.");
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9); // server IP and port
    echoClient.SetAttribute("MaxPackets", UintegerValue(numberOfPackets));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Set up global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Run simulation
    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}