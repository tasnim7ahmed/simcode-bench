#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set TCP segment size to ensure proper RTT measurement
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1024));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Point-to-point link configuration
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install and configure TCP server (PacketSink)
    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Install TCP client (BulkSendApplication)
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("Remote", remoteAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable logging for RTT
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Capture RTT using a trace source
    Config::Connect("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RttTrace",
                    MakeCallback([](const Time& oldVal, const Time& newVal) {
                        std::cout << Simulator::Now().GetSeconds() << "s: RTT changed from "
                                  << oldVal.GetMilliSeconds() << "ms to "
                                  << newVal.GetMilliSeconds() << "ms" << std::endl;
                    }));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}