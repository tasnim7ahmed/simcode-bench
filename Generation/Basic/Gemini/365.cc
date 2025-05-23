#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

int
main()
{
    // Set up default values for Wifi.
    // For 54Mbps, we typically use 802.11a or 802.11g. 802.11a is common for examples.
    // However, 802.11n_5GHZ also supports 54Mbps OFDM rates.
    // Let's use 802.11a for simplicity as it directly maps to OFDM rates without MIMO complexities.
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211a);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                      "DataMode", StringValue("OfdmRate54Mbps"),
                                      "ControlMode", StringValue("OfdmRate6Mbps"));

    // Create two nodes, one for AP and one for STA.
    NodeContainer nodes;
    nodes.Create(2);
    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // Set up mobility: fixed positions for both nodes.
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);
    apNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNode->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(1.0, 0.0, 0.0)); // STA slightly away from AP

    // Create Wi-Fi channel and physical layer.
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> channel = channelHelper.Create();
    YansWifiPhyHelper phyHelper;
    phyHelper.SetChannel(channel);

    // Configure MAC for AP and STA.
    WifiMacHelper macHelper;
    Ssid ssid = Ssid("ns3-wifi");

    // AP configuration
    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "BeaconInterval", TimeValue(NanoSeconds(102400)));
    NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    // STA configuration
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false)); // Disable active probing for simplicity
    NetDeviceContainer staDevice = wifiHelper.Install(phyHelper, macHelper, staNode);

    // Install internet stack on both nodes.
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses.
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = ipv4.Assign(staDevice);

    // Create UDP applications.
    // Server on AP (sink).
    uint16_t port = 9; // Echo port
    Address sinkAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSink("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer serverApps = packetSink.Install(apNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Client on STA (source).
    // Send packets from STA to AP's IP address.
    InetSocketAddress remoteAddress(apInterface.GetAddress(0), port);
    UdpClientHelper udpClient(remoteAddress, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100)); // Send 100 packets
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1))); // One packet every 0.1 seconds
    udpClient.SetAttribute("PacketSize", UintegerValue(1024)); // 1024 bytes per packet
    ApplicationContainer clientApps = udpClient.Install(staNode);
    clientApps.Start(Seconds(1.0)); // Start client after 1 second
    clientApps.Stop(Seconds(9.0)); // Stop client before simulation ends

    // Enable pcap tracing for devices (optional, but useful for debugging).
    phyHelper.EnablePcap("ap", apDevice.Get(0));
    phyHelper.EnablePcap("sta", staDevice.Get(0));

    // Set simulation stop time.
    Simulator::Stop(Seconds(10.0));

    // Run simulation.
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}