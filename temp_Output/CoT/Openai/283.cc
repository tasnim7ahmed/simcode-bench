#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/pcap-file-wrapper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: 3 STAs + 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure WiFi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211g-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                             "Distance", DoubleValue(10.0));
    mobility.Install(wifiStaNodes);

    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Internet stack + IP
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP server on AP (port 4000)
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP clients on each STA
    uint32_t packetSize = 512;
    double interval = 0.01; // 10ms
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
        clientHelper.SetAttribute("MaxPackets", UintegerValue(10000)); // Large number, we'll stop at sim time
        clientHelper.SetAttribute("Interval", TimeValue(Seconds(interval)));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i*0.01));
        clientApp.Stop(Seconds(10.0));
    }

    // Enable PCAP tracing on AP device
    phy.EnablePcap("wifi-ap", apDevice.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}