#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpExample");

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool tracing = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application containers to log if set to true.", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing.", tracing);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(1));

    /* Setting rate adaption algorithm */
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
    address.Assign(apDevices);

    uint16_t port = 9; // well-known echo port number
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    // Client
    uint32_t packetSize = 512;
    uint32_t numPackets = 100;
    Time interPacketInterval = Seconds(0.1);
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("PacketSize", UintegerValue(packetSize));
    source.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));
    source.SetAttribute("SendSize", UintegerValue(packetSize));
    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(20.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (tracing) {
        phy.EnablePcap("WifiTcpExample", apDevices);
    }

    Simulator::Stop(Seconds(20.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}