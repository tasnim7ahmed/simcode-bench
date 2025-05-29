#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/queue-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool tracing = true;
    bool pcap = true;

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable or disable tracing", tracing);
    cmd.AddValue("pcap", "Enable or disable pcap", pcap);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    NodeContainer nodes;
    nodes.Create(4);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("5Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(2)));

    QueueAttributes q;
    q.maxPackets = 20;
    csma.SetQueue(q);

    NetDeviceContainer devices = csma.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper p2p;
    p2p.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = p2p.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer apps = server.Install(nodes.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    apps = client.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    if (tracing) {
        AsciiTraceHelper ascii;
        csma.EnableAsciiAll(ascii.CreateFileStream("udp-echo.tr"));
    }

    if (pcap) {
        csma.EnablePcapAll("udp-echo", false);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}