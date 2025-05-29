#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(3);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs7"),
                                 "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-80211n");

    // STA configuration
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // AP configuration
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    // STA: Grid layout
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    // AP: fixed at (5,10)
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(5.0, 10.0, 0.0));
    mobilityAp.SetPositionAllocator(apPositionAlloc);
    mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install(wifiApNode);

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // UDP Server on the AP
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Clients on each STA
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(5));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));
    }

    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-sta1", staDevices.Get(0));
    phy.EnablePcap("wifi-sta2", staDevices.Get(1));
    phy.EnablePcap("wifi-sta3", staDevices.Get(2));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}