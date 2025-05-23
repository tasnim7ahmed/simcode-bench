#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // 1. Configure default parameters
    double simulationTime = 10.0;     // seconds
    uint32_t payloadSize = 1024;      // bytes
    std::string dataRate = "10Mbps";  // Application data rate
    uint32_t udpPacketCount = 1000;   // Number of UDP packets to send
    double udpInterval = 0.01;        // seconds, interval between UDP packets
    uint32_t blockAckThreshold = 5;   // Number of frames to trigger Block Ack for BE AC
    double blockAckTimeout = 0.1;     // Seconds of inactivity timeout for BE AC

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Payload size of UDP packets in bytes", payloadSize);
    cmd.AddValue("dataRate", "Data rate of UDP packets", dataRate);
    cmd.AddValue("udpPacketCount", "Number of UDP packets to send", udpPacketCount);
    cmd.AddValue("udpInterval", "Time interval between UDP packets", udpInterval);
    cmd.AddValue("blockAckThreshold", "Block Ack threshold for BE AC", blockAckThreshold);
    cmd.AddValue("blockAckTimeout", "Block Ack inactivity timeout for BE AC", blockAckTimeout);
    cmd.Parse(argc, argv);

    // 2. Create nodes: One AP and two STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    NodeContainer allNodes = NodeContainer(wifiApNode, wifiStaNodes);

    // 3. Create YansWifiChannel and YansWifiPhy helpers
    YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    channel.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO); // For full capture details
    channel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.SetErrorRateModel("ns3::YansErrorRateModel");
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO); // For full capture details

    // 4. Configure WifiHelper for 802.11n (HT) capabilities
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n); // Enable 802.11n (HT)

    // Set the remote station manager to a constant rate manager for HT
    // This uses MCS 0, 20MHz bandwidth, No SGI for both data and control frames
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6_5Mbps"),
                                 "ControlMode", StringValue("OfdmRate6_5Mbps"));

    // 5. Configure MAC helpers for AP and STAs
    HtWifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue("ns-3-ssid"),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue("ns-3-ssid"),
                "BeaconInterval", TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // 6. Configure Compressed Block Acknowledgment parameters for the QoS STA (STA0)
    // Get the WifiMac object for the first STA (which will be our QoS STA)
    Ptr<NetDevice> sta0NetDevice = staDevices.Get(0);
    Ptr<WifiNetDevice> sta0WifiNetDevice = DynamicCast<WifiNetDevice>(sta0NetDevice);
    Ptr<WifiMac> sta0WifiMac = sta0WifiNetDevice->GetMac();

    // Set Best Effort (BE) Block Ack threshold and inactivity timeout
    // These attributes control when the MAC layer originates a Block Ack Request (BAR)
    // or terminates an inactive Block Ack session for the Best Effort Access Category.
    sta0WifiMac->SetAttribute("BE_BlockAckThreshold", UintegerValue(blockAckThreshold));
    sta0WifiMac->SetAttribute("BE_BlockAckInactivityTimeout", TimeValue(Seconds(blockAckTimeout)));

    // 7. Setup mobility (ConstantPositionMobilityModel)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    Ptr<ConstantPositionMobilityModel> apMobility = wifiApNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    Ptr<ConstantPositionMobilityModel> sta0Mobility = wifiStaNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    sta0Mobility->SetPosition(Vector(1.0, 0.0, 0.0)); // QoS STA

    Ptr<ConstantPositionMobilityModel> sta1Mobility = wifiStaNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    sta1Mobility->SetPosition(Vector(2.0, 0.0, 0.0)); // Other STA

    // 8. Install Internet Stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // 9. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 10. Configure Applications

    // UDP traffic from QoS STA (staNodes.Get(0)) to AP
    uint16_t udpPort = 9; // Discard port
    UdpClientHelper udpClient(apInterface.GetAddress(0), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(udpPacketCount));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(udpInterval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps = udpClient.Install(wifiStaNodes.Get(0)); // Only STA0 sends UDP
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime - 0.5));

    PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer sinkApps = udpSink.Install(wifiApNode.Get(0));
    sinkApps.Start(Seconds(0.5));
    sinkApps.Stop(Seconds(simulationTime));

    // ICMP replies from AP to QoS STA (staNodes.Get(0))
    // The description implies AP initiates ICMP to the STA.
    PingHelper ping(staInterfaces.GetAddress(0)); // Ping target is QoS STA's IP
    ping.SetAttribute("Count", UintegerValue(10)); // Send 10 ping packets
    ping.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer pingApps = ping.Install(wifiApNode.Get(0)); // AP sends pings
    pingApps.Start(Seconds(2.0));
    pingApps.Stop(Seconds(simulationTime - 0.1));

    // 11. Enable Packet Capture for the AP and QoS STA
    phy.EnablePcap("ap_capture", apDevice.Get(0));
    phy.EnablePcap("sta0_capture", staDevices.Get(0));

    // 12. Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}