#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/icmpv4-echo-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numSta = 2;
    uint32_t qosSta = 0; // index of QoS station (sta0)
    double simulationTime = 10.0;

    // Create nodes: sta0, sta1, ap
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(numSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // PHY and Channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // MAC & Standard (802.11n)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    // QoS MAC for all STAs and AP
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n-bockack");
    // Stations
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false),
                "QosSupported", BooleanValue(true));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);
    // AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "QosSupported", BooleanValue(true));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Configure block acknowledgment (BA) parameters - set BA inactivity timeout and threshold
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BlockAckManager/BlockAckThreshold",
                UintegerValue(12)); // Aggregation threshold
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/$ns3::RegularWifiMac/BlockAckManager/InactivityTimeout",
                TimeValue(MilliSeconds(25))); // BA inactivity timeout

    // Mobility: AP fixed, STAs spread
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(numSta),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(10.0),
                                 "MinY", DoubleValue(5.0),
                                 "DeltaX", DoubleValue(0.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(1),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiApNode);

    // Install TCP/IP stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Static routing for AP (forwarding echoed packets)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Server on AP
    uint16_t udpPort = 9;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApp = udpServer.Install(wifiApNode.Get(0));
    serverApp.Start(Seconds(0.5));
    serverApp.Stop(Seconds(simulationTime));

    // UDP Client (QOS) on STATION 0
    UdpClientHelper udpClient(apInterface.GetAddress(0), udpPort);
    udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(1500)));
    udpClient.SetAttribute("MaxPackets", UintegerValue(40000));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    udpClient.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("StopTime", TimeValue(Seconds(simulationTime-1)));
    udpClient.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(qosSta));

    // ICMPv4 Echo reply from AP to QOS station:
    // Ap acts as ICMP responder; install Echo application replying to the client station
    Icmpv4EchoHelper icmpServer;
    ApplicationContainer icmpServerApps = icmpServer.Install(wifiApNode.Get(0));
    icmpServerApps.Start(Seconds(0.5));
    icmpServerApps.Stop(Seconds(simulationTime));

    Icmpv4EchoHelper icmpClient(apInterface.GetAddress(0));
    icmpClient.SetAttribute("StartTime", TimeValue(Seconds(2.0)));
    icmpClient.SetAttribute("StopTime", TimeValue(Seconds(simulationTime-1)));
    icmpClient.SetAttribute("PacketSize", UintegerValue(100));
    icmpClient.SetAttribute("Interval", TimeValue(Milliseconds(100)));
    ApplicationContainer icmpClientApps = icmpClient.Install(wifiStaNodes.Get(qosSta));

    // Enable pcap tracing at the AP
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("80211n-blockack-ap", apDevice.Get(0), true, true);

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}