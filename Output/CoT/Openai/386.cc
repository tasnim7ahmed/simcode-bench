#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Command line argument parser
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create two nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // WiFi Channel
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    // Set fixed delay
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(-30.0));

    Ptr<YansWifiChannel> channel = wifiChannel.Create();

    // WiFi PHY and MAC
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(channel);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-wifi");

    // Set data rate
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("ErpOfdmRate54Mbps"),
                                "ControlMode", StringValue("ErpOfdmRate24Mbps"));

    NetDeviceContainer devices;

    // Node 0: station, Node 1: AP
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0)));

    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    devices.Add(wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1)));

    // Mobility: constant position, grid placement
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on node 1
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    // UDP client on node 0
    uint32_t maxPacketCount = 320;
    Time interPacketInterval = Seconds(0.00256); // ~10Mbps for 32B packets: 32*8/10Mbps = ~0.0000256s. Let's use realistic
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(256))); // About 10 Mbps for 320 packets of 400 bytes
    client.SetAttribute("PacketSize", UintegerValue(1250)); // 10 Mbps with 0.001s: about 1250 bytes per packet

    ApplicationContainer clientApps = client.Install(wifiNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing for devices
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("wifi-simple", devices);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}