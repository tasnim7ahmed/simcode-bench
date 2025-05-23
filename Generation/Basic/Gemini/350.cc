#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"

// Don't use using namespace ns3; to avoid potential naming conflicts in a library context.

int main(int argc, char *argv[])
{
    // 1. Set TCP congestion control algorithm (NewReno is a common default)
    ns3::Config::SetDefault("ns3::TcpL4Protocol::SocketType", ns3::StringValue("ns3::TcpNewReno"));

    // 2. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 3. Configure Point-to-Point link
    ns3::PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", ns3::StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", ns3::StringValue("2ms"));

    // 4. Install devices on nodes
    ns3::NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 5. Install Internet stack (IP, TCP, UDP)
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 6. Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 7. Configure and install TCP server (PacketSink) on Node 0
    uint16_t port = 9; // Discard port
    ns3::Address sinkLocalAddress(ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ns3::ApplicationContainer serverApp = sinkHelper.Install(nodes.Get(0));
    serverApp.Start(ns3::Seconds(1.0));
    serverApp.Stop(ns3::Seconds(10.0));

    // 8. Configure and install TCP client (OnOffApplication) on Node 1
    // The client sends to the server's IP address (Node 0's IP) and port
    ns3::OnOffHelper onoffHelper("ns3::TcpSocketFactory", ns3::Address(ns3::InetSocketAddress(interfaces.GetAddress(0), port)));
    
    // Client sends 10 packets of size 1024 bytes at a 1-second interval
    // Set PacketSize to 1024 bytes.
    onoffHelper.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    
    // To send 1024 bytes in 1 second, the data rate should be (1024 * 8) bps = 8192 bps.
    // Setting DataRate to this value ensures one packet is sent per second if OnTime is 1s.
    onoffHelper.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate("8192bps")));
    
    // Set OnTime to 1 second and OffTime to 0 to simulate continuous "on" state for 1s sending.
    onoffHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoffHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    
    // Limit the total bytes sent to 10 packets * 1024 bytes/packet = 10240 bytes.
    onoffHelper.SetAttribute("MaxBytes", ns3::UintegerValue(10 * 1024));

    ns3::ApplicationContainer clientApp = onoffHelper.Install(nodes.Get(1));
    clientApp.Start(ns3::Seconds(2.0));
    clientApp.Stop(ns3::Seconds(10.0));

    // 9. Set the simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 10. Run the simulation
    ns3::Simulator::Run();

    // 11. Clean up simulation resources
    ns3::Simulator::Destroy();

    return 0;
}