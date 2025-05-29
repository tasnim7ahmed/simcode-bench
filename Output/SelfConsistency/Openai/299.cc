#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Enable logging if needed (optional)
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Wi-Fi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi MAC and helpers
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    WifiMacHelper mac;

    Ssid ssid = Ssid("wifi-simple");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(1));

    // Set mobility: constant positions
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0));   // Node A
    posAlloc->Add(Vector(5.0, 0.0, 0.0));   // Node B
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(NetDeviceContainer(staDevices, apDevices));

    // Install UDP Server on Node B
    uint16_t serverPort = 4000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP Client on Node A
    uint32_t packetSize = 128;
    uint32_t maxPacketCount = 100; // Enough for duration (every 100ms for 10s = 100 packets)
    Time interPacketInterval = MilliSeconds(100);
    UdpClientHelper udpClient(interfaces.GetAddress(1), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Enable PCAP tracing on Wi-Fi devices
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-simple-nodeA", staDevices.Get(0));
    phy.EnablePcap("wifi-simple-nodeB", apDevices.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}