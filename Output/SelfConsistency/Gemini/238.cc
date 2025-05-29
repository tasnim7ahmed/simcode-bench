#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiMesh");

int main(int argc, char *argv[]) {
    bool enable_pcap = false;
    double simulationTime = 10.0;
    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd;
    cmd.AddValue("EnablePcap", "Enable PCAP Tracing", enable_pcap);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiMac::BE_MaxAmpduSize", UintegerValue(65535));
    Config::SetDefault("ns3::WifiMac::BE_MaxAmpduAggregation", BooleanValue(true));
    Config::SetDefault("ns3::WifiMacQueue::MaxPackets", UintegerValue(1000));

    NodeContainer nodes;
    nodes.Create(2);

    // Set up Wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set up Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(0));
    Ptr<ConstantVelocityMobilityModel> cvmm = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    cvmm->SetVelocity(Vector(5, 0, 0));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes.Get(1));


    // Set up Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP server (sink) on node 1
    uint16_t sinkPort = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime + 1));

    // Set up UDP client on node 0
    uint32_t packetSize = 1024;
    uint32_t maxPackets = 1000;
    Time interPacketInterval = Seconds(0.01);
    UdpClientHelper clientHelper(interfaces.GetAddress(1), sinkPort);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    clientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));


    // Enable PCAP tracing
    if (enable_pcap) {
        wifiPhy.EnablePcapAll("adhoc");
    }

    //AnimationInterface::SetUpdateTime(Seconds(0.1));
    //AnimationInterface anim("adhoc-animation.xml");

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}