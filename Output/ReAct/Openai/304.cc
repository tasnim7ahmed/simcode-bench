#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set simulation parameters
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    double interval = 0.01; // 10 ms
    double startTime = 2.0;
    double stopTime = 10.0;
    uint16_t port = 8080;

    // Create nodes: 2 STAs and 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    // STA MAC configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP MAC configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Install UDP server on STA1
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(stopTime));

    // Install UDP client on STA0 targeting STA1
    UdpClientHelper client(staInterfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(0));
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(stopTime));

    // Enable pcap tracing (optional)
    phy.EnablePcap("wifi-ap", apDevice.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(stopTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}