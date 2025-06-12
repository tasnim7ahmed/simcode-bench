#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set up logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create(4);

    // Create point-to-point links
    PointToPointHelper p2p_n0n2;
    p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n1n2;
    p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p_n2n3;
    p2p_n2n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
    p2p_n2n3.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install devices on the links
    NetDeviceContainer dev_n0n2 = p2p_n0n2.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer dev_n1n2 = p2p_n1n2.Install(nodes.Get(1), nodes.Get(2));
    NetDeviceContainer dev_n2n3 = p2p_n2n3.Install(nodes.Get(2), nodes.Get(3));

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer if_n0n2 = address.Assign(dev_n0n2);
    address.NewNetwork();
    Ipv4InterfaceContainer if_n1n2 = address.Assign(dev_n1n2);
    address.NewNetwork();
    Ipv4InterfaceContainer if_n2n3 = address.Assign(dev_n2n3);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Set up CBR/UDP traffic from n0 to n3
    uint16_t cbrPort_n0n3 = 9;
    Address sinkAddress_n0n3(InetSocketAddress(if_n2n3.GetAddress(1), cbrPort_n0n3));
    PacketSinkHelper packetSink_n0n3("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), cbrPort_n0n3));
    ApplicationContainer sinkApp_n0n3 = packetSink_n0n3.Install(nodes.Get(3));
    sinkApp_n0n3.Start(Seconds(0.0));
    sinkApp_n0n3.Stop(Seconds(10.0));

    OnOffHelper onoff_n0n3("ns3::UdpSocketFactory", sinkAddress_n0n3);
    onoff_n0n3.SetConstantRate(DataRate("448kbps"), 210);
    ApplicationContainer apps_n0n3 = onoff_n0n3.Install(nodes.Get(0));
    apps_n0n3.Start(Seconds(0.5));
    apps_n0n3.Stop(Seconds(10.0));

    // Set up CBR/UDP traffic from n3 to n1
    uint16_t cbrPort_n3n1 = 10;
    Address sinkAddress_n3n1(InetSocketAddress(if_n1n2.GetAddress(1), cbrPort_n3n1));
    PacketSinkHelper packetSink_n3n1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), cbrPort_n3n1));
    ApplicationContainer sinkApp_n3n1 = packetSink_n3n1.Install(nodes.Get(1));
    sinkApp_n3n1.Start(Seconds(0.0));
    sinkApp_n3n1.Stop(Seconds(10.0));

    OnOffHelper onoff_n3n1("ns3::UdpSocketFactory", sinkAddress_n3n1);
    onoff_n3n1.SetConstantRate(DataRate("448kbps"), 210);
    ApplicationContainer apps_n3n1 = onoff_n3n1.Install(nodes.Get(3));
    apps_n3n1.Start(Seconds(0.5));
    apps_n3n1.Stop(Seconds(10.0));

    // Set up FTP/TCP flow from n0 to n3
    uint16_t ftpPort = 21;
    Address sinkAddress_ftp(InetSocketAddress(if_n2n3.GetAddress(1), ftpPort));
    PacketSinkHelper packetSink_ftp("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ftpPort));
    ApplicationContainer sinkApp_ftp = packetSink_ftp.Install(nodes.Get(3));
    sinkApp_ftp.Start(Seconds(1.0));
    sinkApp_ftp.Stop(Seconds(2.0));

    BulkSendHelper bulkSend("ns3::TcpSocketFactory", sinkAddress_ftp);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer apps_ftp = bulkSend.Install(nodes.Get(0));
    apps_ftp.Start(Seconds(1.2));
    apps_ftp.Stop(Seconds(1.35));

    // Enable tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("simple-global-routing.tr");
    p2p_n0n2.EnableAsciiAll(stream);
    p2p_n1n2.EnableAsciiAll(stream);
    p2p_n2n3.EnableAsciiAll(stream);

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}