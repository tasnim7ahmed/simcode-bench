#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Create and configure WiFi channels
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure WiFi standard and helpers
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("wifi-ssid");

    NetDeviceContainer wifiDevices;

    // Configure STA
    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    wifiDevices.Add(
        wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(0)));

    // Configure AP
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    wifiDevices.Add(
        wifi.Install(wifiPhy, wifiMac, wifiNodes.Get(1)));

    // Set mobility: grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces;
    wifiInterfaces = address.Assign(wifiDevices);

    // Install UDP server on node 1 (receiver)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP client on node 0 (sender)
    UdpClientHelper client(wifiInterfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.03125))); // about 32 packets/sec
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing (optional, can be removed)
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap("wifi-simple", wifiDevices);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}