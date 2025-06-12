#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

uint32_t g_rxPackets = 0;
uint32_t g_rxBytes = 0;

void RxCallback(Ptr<const Packet> packet, const Address &address)
{
    g_rxPackets++;
    g_rxBytes += packet->GetSize();
}

int main(int argc, char *argv[])
{
    // Create nodes: 2 STAs + 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi physical and MAC layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> phyChannel = channel.Create();

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(phyChannel);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("DsssRate11Mbps"),
                                "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // Install devices on STAs
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Install device on AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility for nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Enable pcap tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-sta1", staDevices.Get(0));
    phy.EnablePcap("wifi-sta2", staDevices.Get(1));

    // Setup TCP application: Server on STA1, Client on STA0
    uint16_t port = 50000;

    Address serverAddress(InetSocketAddress(staInterfaces.GetAddress(1), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    OnOffHelper client("ns3::TcpSocketFactory", serverAddress);
    client.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    client.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(0));

    // Measure throughput with a packet sink trace
    Ptr<Application> sinkApp = serverApp.Get(0);
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp);
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    double throughput = g_rxBytes * 8.0 / 10.0 / 1e6; // Mbps
    std::cout << "Server received " << g_rxPackets << " packets, "
              << g_rxBytes << " bytes" << std::endl;
    std::cout << "Throughput at server: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}