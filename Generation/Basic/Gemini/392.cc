#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main()
{
    // Configure default TCP protocol type (e.g., NewReno)
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));

    // Define simulation parameters
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";
    double simulationTime = 10.0; // seconds

    // 1. Create two nodes: Node 0 (sender) and Node 1 (receiver)
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Create a point-to-point link
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2pHelper.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices;
    devices = p2pHelper.Install(nodes);

    // 3. Install the Internet stack on both nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // 4. Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 5. Set up TCP applications

    // Receiver (PacketSink) on Node 1
    uint16_t sinkPort = 9; // Port number for the sink application
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1)); // Install on Node 1
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    // Sender (OnOffApplication) on Node 0
    OnOffHelper onoffHelper("ns3::TcpSocketFactory", Address()); // Protocol is TCP
    // Sender will be always "On" (sending continuously)
    onoffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    // Set a high data rate for the OnOff application to ensure TCP's congestion control mechanism works
    onoffHelper.SetAttribute("DataRate", StringValue("10Mbps")); 
    onoffHelper.SetAttribute("PacketSize", UintegerValue(1024)); // Default packet size 1024 bytes

    // Set the remote address for the OnOff application (IP address of Node 1, and the sink port)
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    onoffHelper.SetAttribute("Remote", remoteAddress);

    ApplicationContainer clientApps = onoffHelper.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(Seconds(1.0)); // Start sending after the sink is ready
    clientApps.Stop(Seconds(simulationTime));

    // Optional: Enable PCAP tracing for network devices
    // p2pHelper.EnablePcapAll("tcp-simulation");

    // 6. Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}