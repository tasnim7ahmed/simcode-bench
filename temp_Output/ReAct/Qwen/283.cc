#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiUdpSimulation");

int main(int argc, char *argv[]) {
    uint32_t numStas = 3;
    double simDuration = 10.0;
    uint32_t payloadSize = 512;
    Time interPacketInterval = MilliSeconds(10);

    CommandLine cmd(__FILE__);
    cmd.AddValue("numStas", "Number of station nodes", numStas);
    cmd.AddValue("simDuration", "Simulation duration in seconds", simDuration);
    cmd.Parse(argc, argv);

    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer staNodes;
    staNodes.Create(numStas);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    Ssid ssid = Ssid("wifi-sim");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(staNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(25.0),
                                  "MinY", DoubleValue(25.0),
                                  "DeltaX", DoubleValue(0.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    UdpServerHelper server(4000);
    ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        UdpClientHelper client(apInterface.GetAddress(0), 4000);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("Interval", TimeValue(interPacketInterval));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        ApplicationContainer clientApp = client.Install(staNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simDuration - 0.1));
    }

    phy.EnablePcap("ap-wifi-packet-capture", apDevice.Get(0));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}