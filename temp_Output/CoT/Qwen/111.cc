#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/radvd-interface.h"
#include "ns3/radvd-prefix.h"
#include "ns3/radvd.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RadvdSimulation");

int main(int argc, char *argv[]) {
    Config::SetDefault("ns3::Ipv6L3Protocol::IpForward", BooleanValue(true));
    Config::SetDefault("ns3::Radvd::UnicastOnly", BooleanValue(false));

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer router;
    router.Create(1);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer ndc0 = csma.Install(NodeContainer(nodes.Get(0), router.Get(0)));
    NetDeviceContainer ndc1 = csma.Install(NodeContainer(nodes.Get(1), router.Get(0)));

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.InstallAll();

    Ipv6AddressHelper ipv6;

    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic0 = ipv6.Assign(ndc0);
    iic0.SetForwarding(0, true);
    iic0.SetDefaultRouteInAllNodes(0);

    ipv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer iic1 = ipv6.Assign(ndc1);
    iic1.SetForwarding(0, true);
    iic1.SetDefaultRouteInAllNodes(0);

    RadvdHelper radvdHelper;

    // Interface 0 (towards n0)
    Ptr<RadvdInterface> interface0 = CreateObject<RadvdInterface>(0);
    interface0->SetSendAdvert(true);
    interface0->SetMaxRtrAdvInterval(Seconds(2));
    interface0->SetMinRtrAdvInterval(Seconds(1));
    RadvdPrefix prefix0("2001:1::", Ipv6Prefix(64), Seconds(1800), Seconds(600));
    interface0->AddPrefix(prefix0);
    radvdHelper.AddAnnouncedInterface(interface0);

    // Interface 1 (towards n1)
    Ptr<RadvdInterface> interface1 = CreateObject<RadvdInterface>(1);
    interface1->SetSendAdvert(true);
    interface1->SetMaxRtrAdvInterval(Seconds(2));
    interface1->SetMinRtrAdvInterval(Seconds(1));
    RadvdPrefix prefix1("2001:2::", Ipv6Prefix(64), Seconds(1800), Seconds(600));
    interface1->AddPrefix(prefix1);
    radvdHelper.AddAnnouncedInterface(interface1);

    radvdHelper.Install(router);

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 5;
    Time interPacketInterval = Seconds(1.0);

    Ping6Helper ping;
    ping.SetLocal(iic0.GetAddress(0, 1)); // source address on node 0
    ping.SetRemote(iic1.GetAddress(1, 1)); // destination address on node 1
    ping.SetAttribute("Interval", TimeValue(interPacketInterval));
    ping.SetAttribute("Size", UintegerValue(packetSize));
    ping.SetAttribute("Count", UintegerValue(maxPacketCount));

    ApplicationContainer apps = ping.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("radvd.tr"));
    csma.EnablePcapAll("radvd", false);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}