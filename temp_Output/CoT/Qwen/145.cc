#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    mac.SetType("ns3::MeshPointDeviceMac", "Ssid", SsidValue(Ssid("mesh-network")));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    UdpEchoServerHelper server0(port);
    ApplicationContainer serverApps0 = server0.Install(nodes.Get(0));
    serverApps0.Start(Seconds(1.0));
    serverApps0.Stop(Seconds(10.0));

    UdpEchoClientHelper clientHelper1(interfaces.GetAddress(0), port);
    clientHelper1.SetAttribute("MaxPackets", UintegerValue(20));
    clientHelper1.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps1 = clientHelper1.Install(nodes.Get(1));
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(10.0));

    UdpEchoServerHelper server2(port);
    ApplicationContainer serverApps2 = server2.Install(nodes.Get(2));
    serverApps2.Start(Seconds(1.0));
    serverApps2.Stop(Seconds(10.0));

    UdpEchoClientHelper clientHelper0(interfaces.GetAddress(2), port);
    clientHelper0.SetAttribute("MaxPackets", UintegerValue(20));
    clientHelper0.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    clientHelper0.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps0 = clientHelper0.Install(nodes.Get(0));
    clientApps0.Start(Seconds(3.0));
    clientApps0.Stop(Seconds(10.0));

    phy.EnablePcap("mesh_node0", devices.Get(0), true, true);
    phy.EnablePcap("mesh_node1", devices.Get(1), true, true);
    phy.EnablePcap("mesh_node2", devices.Get(2), true, true);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        NS_LOG_UNCOND("Flow ID: " << iter->first << " Tx Packets = " << iter->second.txPackets << " Rx Packets = " << iter->second.rxPackets);
    }

    Simulator::Destroy();
    return 0;
}