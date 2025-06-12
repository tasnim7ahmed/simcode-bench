#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessLanSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: 1 AP, 2 STAs
    NodeContainer wifiApNode;
    wifiApNode.Create(1);
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    // Set up Wi-Fi with 802.11b standard and data rate of 11 Mbps
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"), "ControlMode", StringValue("DsssRate11Mbps"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(mac, wifiApNode);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(1.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    Ipv4InterfaceContainer staInterfaces;

    apInterface = address.Assign(apDevice);
    staInterfaces = address.Assign(staDevices);

    // TCP server on STA 0 (port 5000)
    uint16_t port = 5000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // TCP client on STA 1 connecting to STA 0
    InetSocketAddress remoteAddress(staInterfaces.GetAddress(0), port);
    remoteAddress.SetTos(0xb8); // DSCP AF41 = 0xb8
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", remoteAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer clientApp = clientHelper.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Enable PCAP tracing
    wifiPhyHelper.EnablePcap("ap-wifi", apDevice.Get(0));
    wifiPhyHelper.EnablePcap("sta0-wifi", staDevices.Get(0));
    wifiPhyHelper.EnablePcap("sta1-wifi", staDevices.Get(1));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output packet counts and throughput
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    std::cout << "Total Bytes Received (Server): " << sink->GetTotalRx() << " bytes" << std::endl;
    std::cout << "Throughput (Server): " << sink->GetTotalRx() * 8.0 / 10.0 / 1e6 << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}