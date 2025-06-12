#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenStationsWiFi");

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t maxAmpduSize = 65535; // default maximum AMPDU size
    double simulationTime = 10.0;
    uint32_t payloadSize = 1000;
    double throughputLowerBound = 0.0;
    double throughputUpperBound = 100.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for Wi-Fi", enableRtsCts);
    cmd.AddValue("maxAmpduSize", "Maximum AMPDU size (bytes)", maxAmpduSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("payloadSize", "Payload size of UDP packets", payloadSize);
    cmd.AddValue("throughputLowerBound", "Minimum expected throughput (Mbps)", throughputLowerBound);
    cmd.AddValue("throughputUpperBound", "Maximum expected throughput (Mbps)", throughputUpperBound);
    cmd.Parse(argc, argv);

    // Create nodes: AP and two stations
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Set up Wi-Fi channel and MAC
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    if (enableRtsCts) {
        mac.Set("RTSThreshold", UintegerValue(1));
    } else {
        mac.Set("RTSThreshold", UintegerValue(999999));
    }

    mac.Set("BE_MaxAmpduSize", UintegerValue(maxAmpduSize));

    // Install AP and station devices
    Ssid ssid = Ssid("hidden-station-network");
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Mobility models to simulate hidden node scenario
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Traffic setup: UDP from both stations to AP
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simulationTime));

    UdpClientHelper clientHelper(apInterface.GetAddress(0), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApp1 = clientHelper.Install(wifiStaNodes.Get(0));
    clientApp1.Start(Seconds(1.0));
    clientApp1.Stop(Seconds(simulationTime));

    ApplicationContainer clientApp2 = clientHelper.Install(wifiStaNodes.Get(1));
    clientApp2.Start(Seconds(1.0));
    clientApp2.Stop(Seconds(simulationTime));

    // PCAP tracing
    phy.EnablePcap("hidden_stations_ap", apDevice.Get(0));
    phy.EnablePcap("hidden_stations_sta1", staDevices.Get(0));
    phy.EnablePcap("hidden_stations_sta2", staDevices.Get(1));

    AsciiTraceHelper asciiTraceHelper;
    phy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("hidden_stations.tr"));

    // Throughput calculation
    Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::UdpServer/Rx", MakeCallback([](Ptr<const Packet> p, const Address &) {
        static uint64_t totalBytes = 0;
        static Time lastTime = Seconds(0);
        totalBytes += p->GetSize();
        Time now = Simulator::Now();
        if ((now - lastTime).GetSeconds() >= 1.0) {
            double throughput = (totalBytes * 8) / (1e6 * (now - lastTime).GetSeconds());
            NS_LOG_UNCOND("Throughput: " << throughput << " Mbps");
            totalBytes = 0;
            lastTime = now;
        }
    }));

    // Simulation run
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}