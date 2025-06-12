#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBlockAckExample");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("blockack-example");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

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

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(staInterfaces.GetAddress(0), port)));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("54Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer apps = onoff.Install(wifiApNode.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(wifiStaNodes.Get(0));
    apps.Start(Seconds(0.0));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_BlockAckInactivityTimeout", TimeValue(MilliSeconds(100)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_BlockAckInactivityTimeout", TimeValue(MilliSeconds(100)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_BlockAckInactivityTimeout", TimeValue(MilliSeconds(100)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BK_BlockAckInactivityTimeout", TimeValue(MilliSeconds(100)));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_BlockAckThreshold", UintegerValue(2));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VO_BlockAckThreshold", UintegerValue(2));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VI_BlockAckThreshold", UintegerValue(2));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BK_BlockAckThreshold", UintegerValue(2));

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("blockack-example.tr");
    phy.EnableAsciiAll(stream);

    phy.EnablePcap("blockack-ap", apDevices.Get(0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}