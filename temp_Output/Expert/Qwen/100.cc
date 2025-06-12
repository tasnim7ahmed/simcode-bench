#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MultiLanWanSimulation");

int main(int argc, char *argv[]) {
    std::string csmaRate = "100Mbps";
    bool enablePcap = true;

    CommandLine cmd;
    cmd.AddValue("csmaRate", "CSMA link rate (e.g., 10Mbps or 100Mbps)", csmaRate);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer topSwitches;
    topSwitches.Create(4); // ts1, ts2, ts3, ts4

    NodeContainer topHosts;
    topHosts.Create(2); // t2, t3

    NodeContainer bottomSwitches;
    bottomSwitches.Create(5); // bs1-bs5

    NodeContainer bottomHosts;
    bottomHosts.Create(2); // b2, b3

    NodeContainer routers;
    routers.Create(2); // tr, br

    CsmaHelper csma;
    if (csmaRate == "100Mbps") {
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    } else {
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    }
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("50ms"));

    NetDeviceContainer topSwitchDevices;
    for (uint32_t i = 0; i < topSwitches.GetN(); ++i) {
        topSwitchDevices.Add(csma.Install(topSwitches.Get(i)));
    }

    NetDeviceContainer bottomSwitchDevices;
    for (uint32_t i = 0; i < bottomSwitches.GetN(); ++i) {
        bottomSwitchDevices.Add(csma.Install(bottomSwitches.Get(i)));
    }

    NetDeviceContainer topHostDevices;
    topHostDevices.Add(csma.Install(topHosts));

    NetDeviceContainer bottomHostDevices;
    bottomHostDevices.Add(csma.Install(bottomHosts));

    NetDeviceContainer routerDevices;
    routerDevices.Add(p2p.Install(routers));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer topInterfaces;
    for (uint32_t i = 0; i < topSwitchDevices.GetN(); ++i) {
        topInterfaces.Add(address.Assign(topSwitchDevices.Get(i)));
    }
    for (uint32_t i = 0; i < topHostDevices.GetN(); ++i) {
        topInterfaces.Add(address.Assign(topHostDevices.Get(i)));
    }

    address.SetBase("192.168.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottomInterfaces;
    for (uint32_t i = 0; i < bottomSwitchDevices.GetN(); ++i) {
        bottomInterfaces.Add(address.Assign(bottomSwitchDevices.Get(i)));
    }
    for (uint32_t i = 0; i < bottomHostDevices.GetN(); ++i) {
        bottomInterfaces.Add(address.Assign(bottomHostDevices.Get(i)));
    }

    address.SetBase("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterfaces = address.Assign(routerDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(bottomHosts.Get(0)); // b2
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    ApplicationContainer serverApps2 = echoServer.Install(bottomHosts.Get(1)); // b3
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(bbottomHosts.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(topHosts.Get(0)); // t2
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(bbottomHosts.Get(1)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = echoClient2.Install(topHosts.Get(1)); // t3
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    if (enablePcap) {
        csma.EnablePcapAll("multi_lan_wan_top_switch", false);
        p2p.EnablePcapAll("multi_lan_wan_wan_link", false);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}