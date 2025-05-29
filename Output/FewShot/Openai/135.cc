#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for the UDP applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create a single node
    NodeContainer node;
    node.Create(1);

    // Create two NetDevices on the same node connected by a point-to-point channel
    NetDeviceContainer devices;

    // Create the point-to-point channel
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // The subtle part: connect node to itself via a point-to-point link
    // Create two devices and assign both to the single node
    Ptr<Node> n = node.Get(0);
    Ptr<NetDevice> devA = pointToPoint.Install(n).Get(0);
    Ptr<NetDevice> devB = pointToPoint.Install(n).Get(0);
    Ptr<Channel> channel = pointToPoint.CreateChannel();
    devA->Attach(channel);
    devB->Attach(channel);
    devices.Add(devA);
    devices.Add(devB);

    // Install the internet stack
    InternetStackHelper stack;
    stack.Install(node);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up the UDP Echo Server on (second interface) of the node
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(node.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the UDP Echo Client on the same node targeting the server address
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(node.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}