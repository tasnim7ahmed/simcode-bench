#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable TCP
    Config::SetDefault("ns3::TcpSocketFactory::TypeId", StringValue("ns3::TcpNewReno"));
    Config::SetDefault("ns3::Ipv4GlobalRoutingHelper::PerflowEcmpRouting", BooleanValue(true));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup WiFi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    // Set default rates for 802.11b
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate11Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));

    // Setup SSID
    Ssid ssid = Ssid("ns-3-ssid");

    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));

    // Create channel and devices
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, apMac, nodes.Get(0));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(1));

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevices);

    // Server application (TCP)
    uint16_t port = 9;
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Client application (TCP)
    Address remoteAddress(InetSocketAddress(staInterface.GetAddress(0), port));
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", remoteAddress);
    ApplicationContainer clientApp = bulkSend.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable ASCII tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wifi-tcp.tr");
    phy.EnableAsciiAll(stream);

    // Enable PCAP tracing
    phy.EnablePcapAll("wifi-tcp");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}