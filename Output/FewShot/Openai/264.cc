#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Configure CSMA channel
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install network devices
    NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP Echo Server on the last node (node 3)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP Echo Client on the first node (node 0)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(3), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging for UdpEcho applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}