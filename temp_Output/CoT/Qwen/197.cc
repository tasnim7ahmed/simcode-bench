#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("P2PTopologyWithTcpAndTracing");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2p1;
    p2p1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p1.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2p2;
    p2p2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p2.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices1, devices2;
    devices1 = p2p1.Install(nodes.Get(0), nodes.Get(1));
    devices2 = p2p2.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces2.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("1Mbps"), 500);
    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(9.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("packet-counts.tr");

    auto txPacketTrace = [stream](std::string context, Ptr<const Packet> packet) {
        *stream->GetStream() << "TX " << context << " Size: " << packet->GetSize() << std::endl;
    };

    auto rxPacketTrace = [stream](std::string context, Ptr<const Packet> packet, const Address&) {
        *stream->GetStream() << "RX " << context << " Size: " << packet->GetSize() << std::endl;
    };

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTx", MakeCallback(txPacketTrace));
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx", MakeCallback(rxPacketTrace));

    p2p1.EnablePcapAll("p2p-traffic", false);
    p2p2.EnablePcapAll("p2p-traffic", false);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}