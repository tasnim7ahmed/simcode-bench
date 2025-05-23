#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    using namespace ns3;

    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Create an Ethernet (CSMA) channel and set its attributes
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("5ms"));

    // 3. Install the CSMA devices on the nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // 4. Install the Internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Configure server application (PacketSink)
    uint16_t port = 50000; // A custom port number
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1)); // Server is Node 1
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // 7. Configure client application (OnOffApplication)
    OnOffHelper onoffHelper("ns3::TcpSocketFactory", Address());
    // Set the remote address to the server's IP and port
    onoffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(1), port)));
    // Configure OnOffHelper to send packets at 0.1s intervals
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always On
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never Off
    onoffHelper.SetAttribute("PacketInterval", StringValue("0.1s")); // Send every 0.1 seconds
    onoffHelper.SetAttribute("DataRate", StringValue("1Mbps")); // Example data rate for the sender
    onoffHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send indefinitely until stopped by application stop time

    ApplicationContainer clientApps = onoffHelper.Install(nodes.Get(0)); // Client is Node 0
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // 8. Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 9. Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // 10. Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}