#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleNetAnimExample");

int main(int argc, char *argv[])
{
    // Enable logging for debugging
    LogComponentEnable("SimpleNetAnimExample", LOG_LEVEL_INFO);

    // Step 1: Create two nodes
    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    // Step 2: Set up a point-to-point link between nodes
    NS_LOG_INFO("Creating point-to-point link.");
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install network devices on the nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Step 3: Install the internet stack on the nodes
    NS_LOG_INFO("Installing internet stack.");
    InternetStackHelper stack;
    stack.Install(nodes);

    // Step 4: Assign IP addresses to the nodes
    NS_LOG_INFO("Assigning IP addresses.");
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Step 5: Set up UDP communication
    NS_LOG_INFO("Setting up UDP server and client.");
    uint16_t port = 9; // port number for the application

    // UDP Server on node 1
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client on node 0
    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Step 6: Enable packet tracing
    NS_LOG_INFO("Enabling packet tracing.");
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("simple-netanim.tr"));

    // Step 7: Set up NetAnim for visualizing the network
    NS_LOG_INFO("Setting up NetAnim.");
    AnimationInterface anim("simple-netanim.xml");

    // Set node positions for visualization
    anim.SetConstantPosition(nodes.Get(0), 0.0, 0.0);  // Node 0 at (0,0)
    anim.SetConstantPosition(nodes.Get(1), 10.0, 0.0); // Node 1 at (10,0)

    // Step 8: Run the simulation
    NS_LOG_INFO("Running the simulation.");
    Simulator::Run();

    // Step 9: Clean up and destroy the simulation
    NS_LOG_INFO("Destroying the simulation.");
    Simulator::Destroy();
    return 0;
}

