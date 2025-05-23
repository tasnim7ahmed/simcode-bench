#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

int main(int argc, char *argv[])
{
    // Set default values for simulation parameters
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);

    // Set simulation time resolution
    Time::SetResolution(NanoSeconds(1));

    // Create four nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create a CSMA helper and set channel attributes
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install CSMA devices on nodes
    NetDeviceContainer devices = csma.Install(nodes);

    // Install the Internet stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the CSMA devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configure TCP Server (Node 0)
    uint16_t port = 8080; // Server port
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(nodes.Get(0)); // Server on Node 0
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Configure TCP Clients (Nodes 1, 2, 3)
    // The server's IP address is interfaces.GetAddress(0) (which is 10.1.1.1)
    BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(0), port));
    // Set MaxBytes to 0 to send indefinitely until application stops
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
    // Set data send rate for clients to 5 Mbps
    clientHelper.SetAttribute("SendRate", DataRateValue(DataRate("5Mbps")));

    ApplicationContainer clientApps;
    clientApps.Add(clientHelper.Install(nodes.Get(1))); // Client on Node 1
    clientApps.Add(clientHelper.Install(nodes.Get(2))); // Client on Node 2
    clientApps.Add(clientHelper.Install(nodes.Get(3))); // Client on Node 3

    // Start clients at 1 second to allow server to initialize
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing for all CSMA devices
    csma.EnablePcapAll("csma-network");

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}