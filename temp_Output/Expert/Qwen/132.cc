#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/dv-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoNodeLoopSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::DvRoutingProtocol::PeriodicUpdateInterval", TimeValue(Seconds(5)));
    Config::SetDefault("ns3::DvRoutingProtocol::SettlingTime", TimeValue(Seconds(10)));

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    InternetStackHelper stack;
    DvRoutingHelper routing;
    stack.SetRoutingHelper(routing);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetConstantDataRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(20.0));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("two-node-loop.tr"));
    p2p.EnablePcapAll("two-node-loop");

    Simulator::Schedule(Seconds(10.0), &Ipv4GlobalRoutingHelper::RecomputeRoutingTables, &routing);

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}