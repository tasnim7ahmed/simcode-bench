#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for block acknowledgment mechanisms
    LogComponentEnable("WifiMacQueue", LOG_LEVEL_INFO);
    LogComponentEnable("BlockAckManager", LOG_LEVEL_INFO);
    LogComponentEnable("QosTxop", LOG_LEVEL_INFO);

    // Create nodes: 1 AP and 2 STAs
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure the channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Setup MAC and PHY for 802.11n
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    // Set QoS to enable Block Ack (BA)
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    // Install MAC layers
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    // Configure AP
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(MicroSeconds(102400)),
                "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode.Get(0));

    // Configure STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Set up mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure Block Ack parameters
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_BlockAckThreshold", UintegerValue(2));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_BlockAckInactivityTimeout", TimeValue(Seconds(30)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_BlockAckInactivityTimeout", TimeValue(Seconds(30)));

    // Setup UDP traffic from STA 0 to AP
    uint16_t udpPort = 4000;
    InetSocketAddress sinkLocalAddress(apInterface.GetAddress(0), udpPort);
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkLocalAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onoff.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Capture packets at AP
    phy.EnablePcap("ap-wifi-packet-capture", apDevice.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}