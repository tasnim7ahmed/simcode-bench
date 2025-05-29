#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Parameters
    uint32_t packetSize = 1024;
    uint32_t numPackets = 100;
    double interval = 0.1; // packet interval in s
    double simulationTime = 10.0; // seconds
    double areaX = 50.0;
    double areaY = 50.0;

    // Create nodes
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Configure Wi-Fi PHY and Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Set Wi-Fi standard and helpers
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    WifiMacHelper mac;
    Ssid ssid = Ssid("simple-wifi");

    // Station MAC/Device
    mac.SetType("ns3::StaWifiMac",
        "Ssid", SsidValue(ssid),
        "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // AP MAC/Device
    mac.SetType("ns3::ApWifiMac",
        "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility for STA - random (RandomWalk2dMobilityModel)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, areaX, 0.0, areaY)),
                              "Distance", DoubleValue(10.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(wifiStaNode);

    // Mobility for AP - fixed
    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // IP address assignment
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP Server (on AP)
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    // UDP Client (on STA)
    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(numPackets));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    // Enable PCAP Tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-simple-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-simple-sta", staDevice.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}