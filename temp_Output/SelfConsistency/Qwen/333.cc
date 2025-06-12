#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerExample");

int main(int argc, char *argv[]) {
    // Enable logging for UdpEchoClient and UdpEchoServer
    LogComponentEnable("TcpSocketBase", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point channel between the nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses from 10.1.1.0/24 subnet
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Setup TCP server application on node 1 (IP: 10.1.1.2)
    uint16_t port = 8080;

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Setup TCP client application on node 0
    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    Config::SetDefault("ns3::TcpClient::Remote", remoteAddress);
    Config::SetDefault("ns3::TcpClient::PacketSize", UintegerValue(1024));
    Config::SetDefault("ns3::TcpClient::MaxPackets", UintegerValue(10));
    Config::SetDefault("ns3::TcpClient::Interval", TimeValue(Seconds(1)));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 1; ++i) {
        Ptr<TcpClient> clientApp = CreateObject<TcpClient>();
        nodes.Get(0)->AddApplication(clientApp);
        clientApp->SetStartTime(Seconds(2.0));
        clientApp->SetStopTime(Seconds(10.0));
        clientApps.Add(clientApp);
    }

    // Set up tracing
    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-client-server.tr"));
    pointToPoint.EnablePcapAll("tcp-client-server");

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}