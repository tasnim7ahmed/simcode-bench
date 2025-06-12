#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/lorawan-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ieee802154-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkPing");

int main(int argc, char *argv[]) {
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
    LogComponentEnable("WirelessSensorNetworkPing", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    NetDeviceContainer devices;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    mac.SetType("ns3::AdhocWifiMac");
    devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper stack;
    stack.SetIpv4StackInstall(false);
    stack.Install(nodes);

    Ipv6AddressHelper address;
    Ipv6InterfaceContainer interfaces;
    address.SetBase(Ipv6Address("2001:db01::"), Ipv6Prefix(64));
    interfaces = address.Assign(devices);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    Icmpv6EchoServerHelper echoServer;
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    Icmpv6EchoClientHelper echoClient(interfaces.GetAddress(1, 1), 0);
    echoClient.SetAttribute("MaxRtt", TimeValue(Seconds(10)));
    echoClient.SetAttribute("MaxFailures", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1)));
    echoClient.SetAttribute("Verbose", BooleanValue(true));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wsn-ping6.tr");
    phy.EnableAsciiAll(stream);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        NS_LOG_INFO("Flow ID: " << iter->first << " Src Addr " << iter->second.txPackets);
    }

    Simulator::Destroy();
    return 0;
}