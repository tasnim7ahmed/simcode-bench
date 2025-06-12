#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211acSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable PCAP tracing
    bool tracing = true;

    // Create nodes: 1 AP and 2 STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    NodeContainer allNodes;
    allNodes.Add(wifiApNode);
    allNodes.Add(wifiStaNodes);

    // Set up Wi-Fi with 802.11ac
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    // Use the default MAC type (regular non-HE)
    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-11ac-network");

    // Configure AP and STA MAC layers
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(mac, wifiStaNodes);

    // Mobility models for STAs (random movement in 50x50 area)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(25.0),
                                  "DeltaY", DoubleValue(25.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(wifiStaNodes);

    // Stationary AP
    MobilityHelper apMobility;
    apMobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue(25.0),
                                    "MinY", DoubleValue(25.0),
                                    "DeltaX", DoubleValue(0),
                                    "DeltaY", DoubleValue(0),
                                    "GridWidth", UintegerValue(1),
                                    "LayoutType", StringValue("RowFirst"));
    apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    apMobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    // Server node is STA 0, client is STA 1
    uint16_t port = 5000;

    // Install TCP server on STA 0
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApp = packetSinkHelper.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Install TCP client on STA 1
    OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApp = onoff.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable PCAP traces
    if (tracing)
    {
        AsciiTraceHelper asciiTraceHelper;
        Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wifi-11ac-simulation.tr");
        wifi.EnableAsciiAll(stream);

        wifi.EnablePcapAll("wifi-11ac-simulation");
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}