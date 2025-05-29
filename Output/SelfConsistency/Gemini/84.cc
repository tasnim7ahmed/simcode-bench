#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/uinteger.h"
#include "ns3/trace-helper.h"
#include "ns3/command-line.h"
#include "ns3/on-off-application.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CsmaTraceFileExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool useIpv6 = false;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("useIpv6", "Use IPv6 addressing if true", useIpv6);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

    NetDeviceContainer devices;
    devices = csma.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv6AddressHelper address6;
    address6.SetBase("2001:db8::", Ipv6Prefix(64));

    Ipv4InterfaceContainer interfaces;
    Ipv6InterfaceContainer interfaces6;

    if (useIpv6) {
        interfaces6 = address6.Assign(devices);
        interfaces6.SetForwarding(0, true);
        interfaces6.SetForwarding(1, true);
        address6.NewNetwork();
    } else {
        interfaces = address.Assign(devices);
    }

    uint16_t port = 9;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    OnOffHelper client("ns3::UdpSocketFactory",
                       Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));


    if (tracing) {
        csma.EnablePcapAll("csma-trace-file-example");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    monitor->CheckForLostPackets ();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
      std::cout << "  Delay Sum:   " << i->second.delaySum << "\n";
      std::cout << "  Jitter Sum:   " << i->second.jitterSum << "\n";
    }

    Simulator::Destroy();

    return 0;
}