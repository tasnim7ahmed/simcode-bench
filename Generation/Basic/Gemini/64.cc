#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/string.h"

#include <iostream>
#include <string>
#include <sstream>

// Use ns3 namespace for convenience
using namespace ns3;

int main(int argc, char *argv[])
{
    // 1. Command-line arguments
    uint32_t numArmNodes = 8;        // Default number of arm nodes (8 arms + 1 hub = 9 total nodes)
    uint32_t packetSize = 1024;      // Default packet size for CBR traffic in bytes
    std::string dataRate = "1Mbps";  // Default data rate for CBR traffic
    double simulationTime = 10.0;    // Default total simulation time in seconds
    uint16_t port = 9;               // TCP port for applications (Echo/PacketSink)

    CommandLine cmd(__FILE__);
    cmd.AddValue("numArmNodes", "Number of arm nodes in the star topology (default: 8). Total nodes will be numArmNodes + 1.", numArmNodes);
    cmd.AddValue("packetSize", "Size of application packets in bytes (default: 1024)", packetSize);
    cmd.AddValue("dataRate", "Data rate of CBR traffic (e.g., '1Mbps', '100Kbps', default: 1Mbps)", dataRate);
    cmd.AddValue("simulationTime", "Total simulation time in seconds (default: 10.0)", simulationTime);
    cmd.Parse(argc, argv);

    // Validate input
    if (numArmNodes == 0)
    {
        NS_FATAL_ERROR("Number of arm nodes must be greater than 0 for a star topology.");
    }

    // 2. Create nodes
    Ptr<Node> hubNode = CreateObject<Node>();
    NodeContainer armNodes;
    armNodes.Create(numArmNodes);

    // 3. Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(armNodes);

    // 4. Configure Point-to-Point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps")); // Link speed for all P2P connections
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));      // Link propagation delay

    // Containers to store hub's NetDevices and IP interfaces for later use (tracing, application config)
    NetDeviceContainer hubNetDevices;
    Ipv4InterfaceContainer hubIpInterfaces;

    for (uint32_t i = 0; i < numArmNodes; ++i)
    {
        NodeContainer linkNodes;
        linkNodes.Add(hubNode);         // Hub node is the first node in the link
        linkNodes.Add(armNodes.Get(i)); // Current arm node is the second

        NetDeviceContainer linkDevices = p2p.Install(linkNodes);

        // Add the hub's end of the link device to the hubNetDevices container
        hubNetDevices.Add(linkDevices.Get(0));

        // Assign IP addresses to the link
        Ipv4AddressHelper ipv4;
        std::ostringstream subnet;
        subnet << "10.1." << (i + 1) << ".0"; // Use a unique subnet for each link (e.g., 10.1.1.0, 10.1.2.0, etc.)
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");

        Ipv4InterfaceContainer linkIpInterfaces = ipv4.Assign(linkDevices);

        // Add the hub's IP interface for this link to the hubIpInterfaces container
        hubIpInterfaces.Add(linkIpInterfaces.Get(0));
    }

    // 5. Install Applications

    // Server (PacketSink) on the hub node
    // It listens on Ipv4Address::GetAny() to receive traffic from all interfaces
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                 InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = sinkHelper.Install(hubNode);
    serverApps.Start(Seconds(1.0));             // Server starts at 1.0s
    serverApps.Stop(Seconds(simulationTime + 1.0)); // Server stops 1.0s after simulationTime to ensure all packets are received

    // Clients (OnOffApplication) on each arm node
    for (uint32_t i = 0; i < numArmNodes; ++i)
    {
        // Get the hub's specific IP address on the link connected to the current arm node
        Ipv4Address hubIpOnThisLink = hubIpInterfaces.Get(i).GetLocalAddress();

        OnOffHelper clientHelper("ns3::TcpSocketFactory",
                                 InetSocketAddress(hubIpOnThisLink, port));
        // Set OnTime to constant 1s and OffTime to constant 0s for Constant Bit Rate (CBR) traffic
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientHelper.SetAttribute("DataRate", DataRateValue(DataRate(dataRate))); // Use ns3::DataRate object

        ApplicationContainer clientApps = clientHelper.Install(armNodes.Get(i));
        clientApps.Start(Seconds(1.0));            // Clients start at 1.0s
        clientApps.Stop(Seconds(simulationTime)); // Clients stop at simulationTime
    }

    // 6. Enable Tracing

    // ASCII Trace: Output queue and packet reception traces to "tcp-star-server.tr"
    // This enables tracing for all point-to-point devices in the simulation.
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii, "tcp-star-server.tr");

    // Pcap Trace: Enable packet capture for each of the hub node's interfaces.
    // The `EnablePcap` overload taking a `NetDevice` pointer automatically generates
    // filenames in the format "prefix-nodeId-interfaceId.pcap".
    // Since hubNode is the first node created, its ID will be 0.
    for (uint32_t i = 0; i < hubNetDevices.GetN(); ++i)
    {
        p2p.EnablePcap("tcp-star-server", hubNetDevices.Get(i));
    }

    // 7. Run Simulation
    Simulator::Stop(Seconds(simulationTime + 2.0)); // Stop the simulation slightly after applications finish
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}