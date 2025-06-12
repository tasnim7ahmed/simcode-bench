#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 10.0;

    // Create 4 nodes: one AP and three STAs
    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(3);

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    // Set up PHY
    YansWifiPhyHelper phy;
    phy.Set("ChannelWidth", UintegerValue(20));

    // Set up channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    // MAC layer configuration
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

    // Install AP and STA devices
    mac.SetType("ns3::ApWifiMac");
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac");
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Mobility models for STAs (random within 50x50 area)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(staNodes);

    // Constant position for AP at (25, 25)
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    apPositionAlloc->Add(Vector(25.0, 25.0, 0.0));
    mobility.SetPositionAllocator(apPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // UDP Server on AP
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(apNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Clients on STAs
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(0)); // Infinite packets
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = udpClient.Install(staNodes.Get(i));
        clientApp.Start(Seconds(0.0));
        clientApp.Stop(Seconds(simTime));
    }

    // Enable PCAP tracing for the AP device
    phy.EnablePcap("ap-wifi-packet-trace", apDevice.Get(0));

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}