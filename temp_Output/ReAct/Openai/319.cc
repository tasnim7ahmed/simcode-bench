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
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numSta = 3;
    double simulationTime = 10.0;
    double serverStart = 1.0;
    double clientStart = 2.0;
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    double interval = 0.01; // seconds

    // Create nodes: STA + AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Wi-Fi channel and devices
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP device
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;

    // STA mobility (randomly placed in 20x20 grid)
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    // AP mobility (fixed at origin)
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(0.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP Server on AP
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(serverStart));
    serverApp.Stop(Seconds(simulationTime));

    // UDP Clients on STAs
    for (uint32_t i = 0; i < numSta; ++i) {
        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(numPackets));
        client.SetAttribute("Interval", TimeValue(Seconds(interval)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(clientStart));
        clientApp.Stop(Seconds(simulationTime));
    }

    // Enable packet capture
    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    for (uint32_t i = 0; i < numSta; ++i) {
        phy.EnablePcap("wifi-sta" + std::to_string(i), staDevices.Get(i));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}