#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    NodeContainer apNode = NodeContainer(wifiNodes.Get(0));
    NodeContainer staNode = NodeContainer(wifiNodes.Get(1));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    // STA
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, staNode);

    // AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));     // AP
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));     // STA
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add(apDevice);
    interfaces.Add(staDevice);

    // UDP server (receiver) on STA (node 1)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(25.0));

    // UDP client (sender) on AP (node 0), send 1000 packets at 20ms intervals
    uint32_t maxPackets = 1000;
    Time interPacketInterval = MilliSeconds(20);
    uint32_t packetSize = 1024;

    UdpClientHelper udpClient(interfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    udpClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(25.0));

    // Enable pcap tracing
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-sta", staDevice.Get(0));

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}